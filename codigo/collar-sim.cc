/*
 * Taller 01 — Simulación de MANETs (ME01 Modelos Estocásticos 2026-02)
 *
 * Collar de perro localizado por RSSI desde una MANET jerárquica de celulares.
 *
 * Jerarquía (posición absoluta = suma de las tres capas):
 *   Nivel 2  zonas      sur, norte                       (ConstantPosition: origen de la zona)
 *   Nivel 1  clústeres  bus-norte, plaza, bus-sur, paseador
 *                        bus   -> WaypointMobilityModel (ruta rectangular con paradas)
 *                        grupo -> RandomWaypointMobilityModel dentro de la zona
 *   Nodo     celulares  RandomWalk2dMobilityModel dentro de la caja del clúster
 *
 * Interruptores del experimento (variable independiente):
 *   --clusterMobility=0/1  ¿se mueven los clústeres (nivel 1)?
 *   --nodeMobility=0/1     ¿se mueven los nodos dentro de su clúster?
 *   --routing=aodv|olsr|dsdv
 *
 * Medición: el collar emite un beacon broadcast 802.11b cada beaconInterval. Cada celular que lo oye
 * registra el RSSI (trace MonitorSnifferRx) y su posición GPS (verdad + ruido gaussiano).
 * Canal: pérdida log-distancia + sombreado normal (en dB) => RSSI log-normal.
 *
 * Red (aplicación UDP sobre el protocolo de enrutamiento elegido):
 *   R  reporte      celular -> cabeza de su clúster         (cada lectura RSSI)
 *   V  relevo       cabeza de clúster -> cabeza de zona
 *   E  intercambio  cabeza de zona -> la otra cabeza de zona (su estimación, cada estInterval)
 *   Q  consulta     dueño -> cabeza de su zona               (cada queryInterval)
 *   A  respuesta    cabeza de zona -> dueño                  (la mejor estimación que tenga)
 *   X  cruda        cabeza de zona -> la otra cabeza de zona (cada lectura; solo --zoneFusion=raw)
 * --zoneFusion=none|raw|merge decide cómo se combinan las dos zonas (ver ZoneFusion).
 *
 * Parámetros: todos por línea de comandos, o en un archivo con --conf=escenarios/base.conf.
 * Cada cabeza de zona estima la posición con las lecturas recibidas en la ventana estWindow, en C++
 * (estimador.h) o, con --estimator=python, en Python a través de ns3-ai (ns3ai/collar-msg.h).
 *
 * Salidas en --outDir: rssi.csv, posiciones.csv, nodos.csv, mensajes.csv, reportes.csv,
 * estimaciones_red.csv, consultas.csv, parametros.txt y (opcional) netanim.xml.
 *
 * Compilar y correr desde codigo/ (ver INSTALACION.md):
 *   ./compilar
 *   ./simular --clusterMobility=1 --nodeMobility=1 --RngRun=1 --outDir=resultados/demo
 */

#include "estimador.h"
#include "ns3ai/collar-msg.h"

// COLLAR_CON_NS3AI lo define codigo/CMakeLists.txt solo si ns-3 compiló el módulo ai.
#ifdef COLLAR_CON_NS3AI
#include "ns3/ns3-ai-msg-interface.h"
#endif

#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-module.h"
#include "ns3/propagation-module.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CollarSim");

namespace
{

// ---------------------------------------------------------------- escenario

const double kZoneWidth = 500.0;  // m
const double kZoneHeight = 150.0; // m; sur ocupa y en [0,150), norte y en [150,300)
const uint16_t kBeaconPort = 9999;
const uint16_t kAppPort = 7000;

enum class ClusterKind
{
    BUS,
    GROUP
};

struct ZoneSpec
{
    std::string name;
    Vector origin;        // esquina inferior izquierda
    uint32_t headCluster; // la cabeza de este clúster es la cabeza de la zona
};

struct ClusterSpec
{
    std::string name;
    uint32_t zone;       // índice en kZones
    ClusterKind kind;
    Vector home;         // posición relativa a la zona cuando el clúster está quieto / arranca
    double halfX;        // semiancho de la caja donde se mueven los nodos (m)
    double halfY;        // semialto
    double nodeSpeed;    // rapidez de los nodos dentro de la caja (m/s)
    uint32_t routeStart; // bus: esquina de la ruta en la que arranca
    uint8_t r, g, b;     // color en NetAnim
};

const std::vector<ZoneSpec> kZones = {
    {"sur", Vector(0, 0, 0), 3},            // cabeza: paseador
    {"norte", Vector(0, kZoneHeight, 0), 1}, // cabeza: plaza
};

// Ruta del bus, relativa al origen de su zona.
const std::vector<Vector> kBusRoute = {
    Vector(50, 30, 0),
    Vector(450, 30, 0),
    Vector(450, 120, 0),
    Vector(50, 120, 0),
};

const std::vector<ClusterSpec> kClusters = {
    {"bus-norte", 1, ClusterKind::BUS, Vector(), 6.0, 1.5, 0.5, 0, 30, 110, 220},
    {"plaza", 1, ClusterKind::GROUP, Vector(250, 75, 0), 10.0, 10.0, 1.0, 0, 40, 170, 90},
    {"bus-sur", 0, ClusterKind::BUS, Vector(), 6.0, 1.5, 0.5, 2, 120, 80, 200},
    {"paseador", 0, ClusterKind::GROUP, Vector(120, 60, 0), 3.0, 3.0, 0.8, 0, 220, 150, 30},
};

// ---------------------------------------------------------------- estado global de la corrida

struct NodeInfo
{
    std::string role; // "observador" | "collar"
    int cluster;      // -1 para el collar
    int zone;         // -1 para el collar
    bool clusterHead;
    bool zoneHead;
    bool owner;
};

struct Estimate
{
    bool valid = false;
    double t = 0;
    double x = 0;
    double y = 0;
    uint32_t observers = 0;
    int zone = -1;
};

// Estado de la aplicación en cada celular.
struct NodeApp
{
    Ptr<Socket> socket;
    Ipv4Address address;
    uint32_t clusterHead = 0; // id de nodo
    uint32_t zoneHead = 0;    // id de nodo
    std::vector<collar::Lectura> lecturas; // solo cabezas de zona
    std::vector<double> lecturaT;           // instante de cada lectura (paralelo a lecturas)
    Estimate own;
    Estimate remote;
};

struct MsgRecord
{
    char type;
    uint32_t from;
    uint32_t to;
    double tSend;
    double tRecv = -1;
    uint32_t bytes;
};

struct ReportRecord
{
    uint32_t origin;
    uint32_t zoneHead;
    double tHeard;
    double gpsX;
    double gpsY;
    double rssi;
    double tZone = -1; // llegada a la cabeza de zona
};

struct QueryRecord
{
    uint32_t owner;
    uint32_t zoneHead;
    double tSend;
    double tReply = -1;
    Estimate answer; // lo que la cabeza de zona contestó
    double truthX = 0;
    double truthY = 0; // posición real del perro cuando llega la respuesta
};

// Cómo se combinan las dos zonas (--zoneFusion).
enum class ZoneFusion
{
    NONE,  // cada cabeza estima solo con su zona; al dueño le llega la mejor de las dos
    RAW,   // cada cabeza reenvía las lecturas crudas de su zona a la otra (mensaje X)
    MERGE  // al contestar, se promedian las dos estimaciones frescas, ponderadas por observadores
};

struct AppConfig
{
    ZoneFusion fusion = ZoneFusion::NONE;
    collar::Metodo method = collar::Metodo::TRILATERACION;
    std::string methodName = "trilateracion";
    double p0 = 0;
    double pathLossExp = 0;
    Time estWindow;
    Time estInterval;
    Time queryInterval;
    double reportJitterMs = 100;
};

std::map<uint32_t, NodeInfo> g_nodeInfo;
std::map<uint32_t, NodeApp> g_app;
std::vector<uint32_t> g_zoneHeads; // por índice de zona
std::map<Mac48Address, uint32_t> g_collarByMac;
std::vector<MsgRecord> g_msgs;       // índice = id de mensaje
std::vector<ReportRecord> g_reports; // índice = id de reporte
std::vector<QueryRecord> g_queries;  // índice = id de consulta
AppConfig g_cfg;
// Canal ns3-ai hacia el estimador en Python; nulo si --estimator=cpp.
#ifdef COLLAR_CON_NS3AI
Ns3AiMsgInterfaceImpl<collar::Peticion, collar::Respuesta>* g_ai = nullptr;
#endif
uint64_t g_aiTruncated = 0; // ventanas con más de kMaxLecturas lecturas
Ptr<MobilityModel> g_dogMm;
AnimationInterface* g_anim = nullptr;
std::vector<Ptr<Node>> g_estimationMarkerNodes;
std::vector<Ptr<ConstantPositionMobilityModel>> g_estimationMarkerModels;
std::vector<bool> g_estimationMarkerVisible;
Ptr<NormalRandomVariable> g_gpsNoise;
Ptr<UniformRandomVariable> g_appJitter;
std::ofstream g_rssiOut;
std::ofstream g_posOut;
std::ofstream g_estOut;
uint64_t g_beaconsSent = 0;

uint32_t
NodeIdFromContext(const std::string& context)
{
    // context = "/NodeList/<id>/DeviceList/..."
    std::size_t start = context.find("/NodeList/") + 10;
    std::size_t end = context.find('/', start);
    return std::stoul(context.substr(start, end - start));
}

// ---------------------------------------------------------------- mensajes de aplicación

void HandleMsg(uint32_t node, std::string payload);

// Envía un mensaje de aplicación; si origen == destino se entrega localmente.
void
SendMsg(uint32_t from, uint32_t to, char type, const std::string& body)
{
    uint32_t id = g_msgs.size();
    std::string payload = std::to_string(id) + "|" + type + "|" + body;
    g_msgs.push_back({type, from, to, Simulator::Now().GetSeconds(), -1, uint32_t(payload.size())});
    if (from == to)
    {
        Simulator::ScheduleNow(&HandleMsg, to, payload);
        return;
    }
    Ptr<Packet> p = Create<Packet>(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    g_app[from].socket->SendTo(p, 0, InetSocketAddress(g_app[to].address, kAppPort));
}

void
ReceiveMsg(Ptr<Socket> socket)
{
    Address from;
    while (Ptr<Packet> p = socket->RecvFrom(from))
    {
        std::string payload(p->GetSize(), '\0');
        p->CopyData(reinterpret_cast<uint8_t*>(payload.data()), payload.size());
        HandleMsg(socket->GetNode()->GetId(), payload);
    }
}

std::string
ReportBody(uint32_t rid)
{
    const ReportRecord& r = g_reports[rid];
    std::ostringstream body;
    body << rid << "," << r.origin << "," << r.tHeard << "," << r.gpsX << "," << r.gpsY << ","
         << r.rssi;
    return body.str();
}

void
AddReading(uint32_t zoneHead, uint32_t rid)
{
    const ReportRecord& r = g_reports[rid];
    NodeApp& app = g_app[zoneHead];
    app.lecturas.push_back({r.origin, r.gpsX, r.gpsY, r.rssi});
    app.lecturaT.push_back(r.tHeard);
}

// Llega una lectura de la propia zona a su cabeza.
void
DeliverReport(uint32_t zoneHead, uint32_t rid)
{
    ReportRecord& r = g_reports[rid];
    if (r.tZone >= 0)
    {
        return; // duplicado
    }
    r.tZone = Simulator::Now().GetSeconds();
    AddReading(zoneHead, rid);
    if (g_cfg.fusion == ZoneFusion::RAW)
    {
        for (uint32_t other : g_zoneHeads)
        {
            if (other != zoneHead)
            {
                SendMsg(zoneHead, other, 'X', ReportBody(rid));
            }
        }
    }
}

// La mejor estimación que tiene una cabeza de zona: si las dos están frescas (edad <= estWindow),
// la de más observadores (o, con --zoneFusion=merge, su promedio ponderado); si no, la más reciente.
Estimate
BestEstimate(const NodeApp& app)
{
    double now = Simulator::Now().GetSeconds();
    double window = g_cfg.estWindow.GetSeconds();
    const Estimate& a = app.own;
    const Estimate& b = app.remote;
    bool aFresh = a.valid && now - a.t <= window;
    bool bFresh = b.valid && now - b.t <= window;
    if (aFresh && bFresh && g_cfg.fusion == ZoneFusion::MERGE)
    {
        Estimate m;
        m.valid = true;
        m.t = std::min(a.t, b.t); // la edad de la fusión es la de la más vieja
        m.observers = a.observers + b.observers;
        m.x = (a.x * a.observers + b.x * b.observers) / m.observers;
        m.y = (a.y * a.observers + b.y * b.observers) / m.observers;
        m.zone = -1; // fusionada
        return m;
    }
    if (aFresh && bFresh)
    {
        if (a.observers != b.observers)
        {
            return a.observers > b.observers ? a : b;
        }
        return a.t >= b.t ? a : b;
    }
    if (a.valid && b.valid)
    {
        return a.t >= b.t ? a : b;
    }
    return a.valid ? a : b;
}

void
HandleMsg(uint32_t node, std::string payload)
{
    std::size_t p1 = payload.find('|');
    uint32_t id = std::stoul(payload.substr(0, p1));
    char type = payload[p1 + 1];
    std::string body = payload.substr(p1 + 3);
    if (g_msgs[id].tRecv < 0)
    {
        g_msgs[id].tRecv = Simulator::Now().GetSeconds();
    }
    NodeApp& app = g_app[node];

    switch (type)
    {
    case 'R': { // reporte: llega a la cabeza de clúster
        uint32_t rid = std::stoul(body.substr(0, body.find(',')));
        if (app.zoneHead == node)
        {
            DeliverReport(node, rid);
        }
        else
        {
            SendMsg(node, app.zoneHead, 'V', body);
        }
        break;
    }
    case 'V': { // relevo: llega a la cabeza de zona
        DeliverReport(node, std::stoul(body.substr(0, body.find(','))));
        break;
    }
    case 'X': { // lectura cruda de la otra zona (--zoneFusion=raw); no se vuelve a reenviar
        AddReading(node, std::stoul(body.substr(0, body.find(','))));
        break;
    }
    case 'E': { // intercambio entre cabezas de zona: zona,t,x,y,observadores
        std::istringstream in(body);
        std::string f;
        Estimate e;
        e.valid = true;
        std::getline(in, f, ',');
        e.zone = std::stoi(f);
        std::getline(in, f, ',');
        e.t = std::stod(f);
        std::getline(in, f, ',');
        e.x = std::stod(f);
        std::getline(in, f, ',');
        e.y = std::stod(f);
        std::getline(in, f, ',');
        e.observers = std::stoul(f);
        if (!app.remote.valid || e.t > app.remote.t)
        {
            app.remote = e;
        }
        break;
    }
    case 'Q': { // consulta: la cabeza de zona contesta con lo mejor que tenga
        uint32_t qid = std::stoul(body);
        QueryRecord& q = g_queries[qid];
        q.answer = BestEstimate(app);
        std::ostringstream reply;
        reply << qid << "," << q.answer.valid << "," << q.answer.t << "," << q.answer.x << ","
              << q.answer.y << "," << q.answer.observers << "," << q.answer.zone;
        SendMsg(node, q.owner, 'A', reply.str());
        break;
    }
    case 'A': { // respuesta: llega al dueño
        uint32_t qid = std::stoul(body.substr(0, body.find(',')));
        QueryRecord& q = g_queries[qid];
        if (q.tReply < 0)
        {
            q.tReply = Simulator::Now().GetSeconds();
            Vector dog = g_dogMm->GetPosition();
            q.truthX = dog.x;
            q.truthY = dog.y;
        }
        break;
    }
    }
}

// El celular manda su lectura a la cabeza de su clúster (o la procesa si él mismo es la cabeza).
void
SendReport(uint32_t observer, uint32_t rid)
{
    SendMsg(observer, g_app[observer].clusterHead, 'R', ReportBody(rid));
}

#ifdef COLLAR_CON_NS3AI
// Manda la ventana al estimador en Python por memoria compartida y espera la respuesta.
// La simulación queda detenida mientras Python calcula.
collar::Resultado
EstimarEnPython(const std::vector<collar::Lectura>& lecturas,
                const std::vector<double>& tLecturas,
                uint32_t zone,
                uint32_t zoneHead)
{
    std::size_t start = 0;
    if (lecturas.size() > collar::kMaxLecturas)
    {
        start = lecturas.size() - collar::kMaxLecturas; // las más recientes
        ++g_aiTruncated;
    }
    g_ai->CppSendBegin();
    collar::Peticion* p = g_ai->GetCpp2PyStruct();
    p->metodo = static_cast<uint32_t>(g_cfg.method);
    p->zona = zone;
    p->cabezaZona = zoneHead;
    p->t = Simulator::Now().GetSeconds();
    p->p0 = g_cfg.p0;
    p->n = g_cfg.pathLossExp;
    p->num = lecturas.size() - start;
    for (std::size_t i = start; i < lecturas.size(); ++i)
    {
        std::size_t k = i - start;
        p->observador[k] = lecturas[i].observador;
        p->tLectura[k] = tLecturas[i];
        p->x[k] = lecturas[i].x;
        p->y[k] = lecturas[i].y;
        p->rssi[k] = lecturas[i].rssi;
    }
    g_ai->CppSendEnd();

    g_ai->CppRecvBegin();
    const collar::Respuesta* r = g_ai->GetPy2CppStruct();
    collar::Resultado res{r->valida != 0, r->x, r->y};
    g_ai->CppRecvEnd();
    return res;
}
#endif

void
EstimateTick(uint32_t zoneHead)
{
    NodeApp& app = g_app[zoneHead];
    double now = Simulator::Now().GetSeconds();
    double from = now - g_cfg.estWindow.GetSeconds();

    // Descarta lecturas viejas y estima con las de la ventana.
    std::vector<collar::Lectura> keep;
    std::vector<double> keepT;
    for (std::size_t i = 0; i < app.lecturas.size(); ++i)
    {
        if (app.lecturaT[i] > from)
        {
            keep.push_back(app.lecturas[i]);
            keepT.push_back(app.lecturaT[i]);
        }
    }
    app.lecturas = keep;
    app.lecturaT = keepT;

    std::set<uint32_t> observers;
    for (const auto& l : keep)
    {
        observers.insert(l.observador);
    }
    int zone = g_nodeInfo[zoneHead].zone;
#ifdef COLLAR_CON_NS3AI
    collar::Resultado res =
        g_ai ? EstimarEnPython(keep, keepT, zone, zoneHead)
             : collar::Estimar(keep, g_cfg.method, g_cfg.p0, g_cfg.pathLossExp);
#else
    collar::Resultado res = collar::Estimar(keep, g_cfg.method, g_cfg.p0, g_cfg.pathLossExp);
#endif
    Vector dog = g_dogMm->GetPosition();
    g_estOut << now << "," << zone << "," << zoneHead << "," << g_cfg.methodName << "," << res.valido
             << ",";
    if (res.valido)
    {
        g_estOut << res.x << "," << res.y << "," << dog.x << "," << dog.y << ","
                 << std::hypot(res.x - dog.x, res.y - dog.y);
    }
    else
    {
        g_estOut << ",," << dog.x << "," << dog.y << ",";
    }
    g_estOut << "," << keep.size() << "," << observers.size() << "\n";

    if (g_anim && zone >= 0 && static_cast<std::size_t>(zone) < g_estimationMarkerModels.size())
    {
        std::size_t markerIndex = static_cast<std::size_t>(zone);
        Ptr<Node> marker = g_estimationMarkerNodes[markerIndex];
        if (res.valido)
        {
            g_estimationMarkerModels[markerIndex]->SetPosition(Vector(res.x, res.y, 0));
            if (!g_estimationMarkerVisible[markerIndex])
            {
                g_anim->UpdateNodeColor(marker,
                                        markerIndex == 0 ? 255 : 0,
                                        markerIndex == 0 ? 0 : 210,
                                        markerIndex == 0 ? 255 : 210);
                g_anim->UpdateNodeDescription(marker,
                                              markerIndex == 0 ? "EST-S" : "EST-N");
                g_estimationMarkerVisible[markerIndex] = true;
            }
        }
        else if (g_estimationMarkerVisible[markerIndex])
        {
            Vector zoneCenter(kZoneWidth / 2,
                              kZones[markerIndex].origin.y + kZoneHeight / 2,
                              0);
            g_estimationMarkerModels[markerIndex]->SetPosition(zoneCenter);
            g_anim->UpdateNodeColor(marker, 130, 130, 130);
            g_anim->UpdateNodeDescription(marker,
                                          markerIndex == 0 ? "EST-S sin dato" : "EST-N sin dato");
            g_estimationMarkerVisible[markerIndex] = false;
        }
    }

    if (res.valido)
    {
        app.own = {true, now, res.x, res.y, uint32_t(observers.size()), zone};
        std::ostringstream body;
        body << zone << "," << now << "," << res.x << "," << res.y << "," << observers.size();
        for (uint32_t other : g_zoneHeads)
        {
            if (other != zoneHead)
            {
                SendMsg(zoneHead, other, 'E', body.str());
            }
        }
    }
    Simulator::Schedule(g_cfg.estInterval, &EstimateTick, zoneHead);
}

void
QueryTick(uint32_t owner)
{
    uint32_t qid = g_queries.size();
    QueryRecord q;
    q.owner = owner;
    q.zoneHead = g_app[owner].zoneHead;
    q.tSend = Simulator::Now().GetSeconds();
    g_queries.push_back(q);
    SendMsg(owner, q.zoneHead, 'Q', std::to_string(qid));
    // Intervalo uniforme en [0.5, 1.5] x queryInterval: si fuera fijo, las consultas caerían siempre
    // en el mismo punto del ciclo de estimación y la edad de la respuesta saldría constante.
    double mean = g_cfg.queryInterval.GetSeconds();
    Simulator::Schedule(Seconds(g_appJitter->GetValue(0.5 * mean, 1.5 * mean)), &QueryTick, owner);
}

// ---------------------------------------------------------------- medición

// Se llama cada vez que un PHY decodifica una trama. Solo interesan las del collar.
void
SnifferRx(std::string context,
          Ptr<const Packet> packet,
          uint16_t channelFreqMhz,
          WifiTxVector txVector,
          MpduInfo aMpdu,
          SignalNoiseDbm signalNoise,
          uint16_t staId)
{
    WifiMacHeader hdr;
    Ptr<Packet> copy = packet->Copy();
    if (copy->PeekHeader(hdr) == 0 || !hdr.IsData())
    {
        return;
    }
    auto collarIt = g_collarByMac.find(hdr.GetAddr2());
    if (collarIt == g_collarByMac.end())
    {
        return;
    }
    uint32_t observerId = NodeIdFromContext(context);
    const NodeInfo& info = g_nodeInfo[observerId];
    if (info.role != "observador")
    {
        return;
    }

    Vector obs = NodeList::GetNode(observerId)->GetObject<MobilityModel>()->GetPosition();
    Vector dog = g_dogMm->GetPosition();
    double gpsX = obs.x + g_gpsNoise->GetValue();
    double gpsY = obs.y + g_gpsNoise->GetValue();
    double dist = CalculateDistance(obs, dog);
    double now = Simulator::Now().GetSeconds();

    g_rssiOut << now << "," << observerId << "," << info.cluster << "," << info.zone << "," << obs.x
              << "," << obs.y << "," << gpsX << "," << gpsY << "," << collarIt->second << ","
              << dog.x << "," << dog.y << "," << signalNoise.signal << "," << dist << "\n";

    // La lectura sale por la red con un pequeño retardo aleatorio (evita que todos los celulares
    // que oyeron el mismo beacon transmitan a la vez).
    uint32_t rid = g_reports.size();
    g_reports.push_back(
        {observerId, g_app[observerId].zoneHead, now, gpsX, gpsY, signalNoise.signal, -1});
    Simulator::Schedule(MilliSeconds(g_appJitter->GetValue(0, g_cfg.reportJitterMs)),
                        &SendReport,
                        observerId,
                        rid);
}

void
SendBeacon(Ptr<Socket> socket, Time interval, Ptr<UniformRandomVariable> jitter, uint32_t size)
{
    socket->Send(Create<Packet>(size));
    ++g_beaconsSent;
    Time next = interval + MilliSeconds(jitter->GetValue(-50, 50));
    Simulator::Schedule(next, &SendBeacon, socket, interval, jitter, size);
}

void
LogPositions(NodeContainer nodes, Time interval)
{
    double now = Simulator::Now().GetSeconds();
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        Vector p = (*it)->GetObject<MobilityModel>()->GetPosition();
        const NodeInfo& info = g_nodeInfo[(*it)->GetId()];
        g_posOut << now << "," << (*it)->GetId() << "," << info.role << "," << info.cluster << ","
                 << info.zone << "," << p.x << "," << p.y << "\n";
    }
    Simulator::Schedule(interval, &LogPositions, nodes, interval);
}

// Waypoints de un bus: recorre la ruta en bucle a rapidez constante, con parada en cada esquina.
Ptr<WaypointMobilityModel>
MakeBusRoute(uint32_t startCorner, double speed, double dwell, double simTime)
{
    Ptr<WaypointMobilityModel> wp = CreateObject<WaypointMobilityModel>();
    double t = 0.0;
    uint32_t i = startCorner % kBusRoute.size();
    wp->AddWaypoint(Waypoint(Seconds(t), kBusRoute[i]));
    while (t < simTime)
    {
        t += dwell;
        wp->AddWaypoint(Waypoint(Seconds(t), kBusRoute[i])); // sale de la parada
        uint32_t next = (i + 1) % kBusRoute.size();
        t += CalculateDistance(kBusRoute[i], kBusRoute[next]) / speed;
        wp->AddWaypoint(Waypoint(Seconds(t), kBusRoute[next]));
        i = next;
    }
    return wp;
}

// Expande cada --conf=<archivo> en los parámetros del archivo, en ese mismo punto de la línea de
// comandos: lo que venga después en la línea gana. Formato: una «clave = valor» por línea; '#' comenta.
std::vector<std::string>
ExpandConf(int argc, char* argv[], std::vector<std::string>& confFiles)
{
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a.rfind("--conf=", 0) != 0)
        {
            args.push_back(a);
            continue;
        }
        std::string path = a.substr(7);
        std::ifstream in(path);
        NS_ABORT_MSG_IF(!in, "no se pudo abrir el archivo de parámetros " << path);
        confFiles.push_back(path);
        std::string line;
        while (std::getline(in, line))
        {
            line = line.substr(0, line.find('#'));
            std::size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }
            auto trim = [](std::string s) {
                std::size_t b = s.find_first_not_of(" \t\r");
                std::size_t e = s.find_last_not_of(" \t\r");
                return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
            };
            std::string key = trim(line.substr(0, eq));
            std::string value = trim(line.substr(eq + 1));
            if (!key.empty())
            {
                args.push_back("--" + key + "=" + value);
            }
        }
    }
    return args;
}

void
WriteResults(const std::string& outDir)
{
    std::ofstream msgs(outDir + "/mensajes.csv");
    msgs << "mensaje,tipo,origen,destino,t_envio,t_llegada,bytes\n";
    for (std::size_t i = 0; i < g_msgs.size(); ++i)
    {
        const MsgRecord& m = g_msgs[i];
        msgs << i << "," << m.type << "," << m.from << "," << m.to << "," << m.tSend << ",";
        if (m.tRecv >= 0)
        {
            msgs << m.tRecv;
        }
        msgs << "," << m.bytes << "\n";
    }

    std::ofstream reps(outDir + "/reportes.csv");
    reps << "reporte,origen,cabeza_zona,t_lectura,t_en_cabeza_zona\n";
    for (std::size_t i = 0; i < g_reports.size(); ++i)
    {
        const ReportRecord& r = g_reports[i];
        reps << i << "," << r.origin << "," << r.zoneHead << "," << r.tHeard << ",";
        if (r.tZone >= 0)
        {
            reps << r.tZone;
        }
        reps << "\n";
    }

    std::ofstream qs(outDir + "/consultas.csv");
    qs << "consulta,dueno,cabeza_zona,t_envio,t_respuesta,con_estimacion,t_estimacion,zona_estimacion,"
          "observadores,x,y,verdad_x,verdad_y,error_m\n";
    for (std::size_t i = 0; i < g_queries.size(); ++i)
    {
        const QueryRecord& q = g_queries[i];
        qs << i << "," << q.owner << "," << q.zoneHead << "," << q.tSend << ",";
        if (q.tReply < 0)
        {
            qs << ",,,,,,,,,\n";
            continue;
        }
        qs << q.tReply << "," << q.answer.valid << ",";
        if (q.answer.valid)
        {
            qs << q.answer.t << "," << q.answer.zone << "," << q.answer.observers << ","
               << q.answer.x << "," << q.answer.y << "," << q.truthX << "," << q.truthY << ","
               << std::hypot(q.answer.x - q.truthX, q.answer.y - q.truthY) << "\n";
        }
        else
        {
            qs << ",,,,," << q.truthX << "," << q.truthY << ",\n";
        }
    }
}

} // namespace

int
main(int argc, char* argv[])
{
    bool clusterMobility = true;
    bool nodeMobility = true;
    uint32_t nodesPerCluster = 8;
    double simTime = 600.0;
    double busSpeed = 6.0;
    double busDwell = 20.0;
    double groupSpeedMin = 0.5;
    double groupSpeedMax = 1.5;
    double groupPause = 30.0;
    bool dogMobility = true;
    double dogX = 300.0;
    double dogY = 140.0;
    double dogSpeed = 0.3;
    double beaconInterval = 1.0;
    uint32_t beaconSize = 32;
    double collarTxDbm = 0.0;
    double phoneTxDbm = 16.0;
    double rxSensitivityDbm = -95.0;
    double pathLossExp = 2.7;
    double refLossDb = 40.05; // pérdida a 1 m en 2.4 GHz (Friis)
    double shadowSigmaDb = 4.0;
    double gpsSigma = 4.0;
    double posLogInterval = 1.0;
    std::string routing = "aodv";
    std::string phoneDataMode = "DsssRate1Mbps";
    std::string estMethod = "trilateracion";
    std::string estimator = "cpp";
    std::string ns3aiTrial = "single_trial";
    std::string zoneFusion = "none";
    double estWindow = 10.0;
    double estInterval = 5.0;
    double queryInterval = 10.0;
    double queryStart = 20.0;
    uint32_t ownerCluster = 0;
    double reportJitterMs = 100.0;
    uint32_t arpQueue = 64;
    bool netanim = false;
    std::string outDir = "resultados/prueba";

    CommandLine cmd(__FILE__);
    cmd.AddValue("clusterMobility", "Clústeres (nivel 1) en movimiento", clusterMobility);
    cmd.AddValue("nodeMobility", "Nodos en movimiento dentro de su clúster", nodeMobility);
    cmd.AddValue("nodesPerCluster", "Celulares por clúster (>= 2)", nodesPerCluster);
    cmd.AddValue("simTime", "Duración de la simulación (s)", simTime);
    cmd.AddValue("busSpeed", "Rapidez de los buses (m/s)", busSpeed);
    cmd.AddValue("busDwell", "Parada del bus en cada esquina (s)", busDwell);
    cmd.AddValue("groupSpeedMin", "Rapidez mínima de los grupos a pie (m/s)", groupSpeedMin);
    cmd.AddValue("groupSpeedMax", "Rapidez máxima de los grupos a pie (m/s)", groupSpeedMax);
    cmd.AddValue("groupPause", "Pausa máxima de los grupos entre destinos (s)", groupPause);
    cmd.AddValue("dogMobility", "El perro perdido deambula", dogMobility);
    cmd.AddValue("dogX", "Posición inicial x del perro (m)", dogX);
    cmd.AddValue("dogY", "Posición inicial y del perro (m)", dogY);
    cmd.AddValue("dogSpeed", "Rapidez del perro (m/s)", dogSpeed);
    cmd.AddValue("beaconInterval", "Periodo del beacon del collar (s)", beaconInterval);
    cmd.AddValue("beaconSize", "Carga útil del beacon (bytes)", beaconSize);
    cmd.AddValue("collarTxDbm", "Potencia de transmisión del collar (dBm)", collarTxDbm);
    cmd.AddValue("phoneTxDbm", "Potencia de transmisión de los celulares (dBm)", phoneTxDbm);
    cmd.AddValue("rxSensitivityDbm", "Sensibilidad de recepción (dBm)", rxSensitivityDbm);
    cmd.AddValue("pathLossExp", "Exponente de pérdida log-distancia", pathLossExp);
    cmd.AddValue("refLossDb", "Pérdida a 1 m (dB)", refLossDb);
    cmd.AddValue("shadowSigmaDb", "Desviación del sombreado (dB); 0 = sin sombreado", shadowSigmaDb);
    cmd.AddValue("gpsSigma", "Desviación del error GPS por eje (m)", gpsSigma);
    cmd.AddValue("posLogInterval", "Periodo de registro de posiciones (s)", posLogInterval);
    cmd.AddValue("routing", "Enrutamiento: aodv | olsr | dsdv", routing);
    cmd.AddValue("phoneDataMode", "Modo 802.11b de los celulares (DsssRate1Mbps ... DsssRate11Mbps)",
                 phoneDataMode);
    cmd.AddValue("estMethod", "Estimador en la cabeza de zona: cercano | centroide | trilateracion",
                 estMethod);
    cmd.AddValue("estWindow", "Ventana de lecturas para estimar (s)", estWindow);
    cmd.AddValue("estInterval", "Cada cuánto estima la cabeza de zona (s)", estInterval);
    cmd.AddValue("queryInterval", "Cada cuánto consulta el dueño (s)", queryInterval);
    cmd.AddValue("queryStart", "Primera consulta del dueño (s)", queryStart);
    cmd.AddValue("ownerCluster", "Clúster del dueño (0 bus-norte, 1 plaza, 2 bus-sur, 3 paseador)",
                 ownerCluster);
    cmd.AddValue("reportJitterMs", "Retardo aleatorio máximo antes de enviar una lectura (ms)",
                 reportJitterMs);
    cmd.AddValue("zoneFusion",
                 "Cómo combinar las dos zonas: none (cada una por su lado) | raw (reenviar lecturas "
                 "crudas entre cabezas) | merge (promediar las dos estimaciones)",
                 zoneFusion);
    std::string confHelp; // solo para que --help lo muestre; ExpandConf ya lo procesó
    cmd.AddValue("conf",
                 "Archivo de parámetros clave = valor (ver escenarios/base.conf); lo que venga "
                 "después en la línea de comandos tiene prioridad",
                 confHelp);
    cmd.AddValue("estimator",
                 "Dónde corre el estimador de las cabezas de zona: cpp | python (ns3-ai; lanzar con "
                 "analisis/ns3ai_estimador.py)",
                 estimator);
    cmd.AddValue("ns3aiTrial", "Nombre de la memoria compartida de ns3-ai (único por corrida en paralelo)",
                 ns3aiTrial);
    cmd.AddValue("arpQueue",
                 "Paquetes en espera por vecino sin resolver en ARP (ns-3 trae 3; Linux ~100)",
                 arpQueue);
    cmd.AddValue("netanim", "Escribir netanim.xml", netanim);
    cmd.AddValue("outDir", "Carpeta de salida (relativa a la carpeta actual)", outDir);
    std::vector<std::string> confFiles;
    cmd.Parse(ExpandConf(argc, argv, confFiles));
    if (zoneFusion == "none")
    {
        g_cfg.fusion = ZoneFusion::NONE;
    }
    else if (zoneFusion == "raw")
    {
        g_cfg.fusion = ZoneFusion::RAW;
    }
    else if (zoneFusion == "merge")
    {
        g_cfg.fusion = ZoneFusion::MERGE;
    }
    else
    {
        NS_ABORT_MSG("zoneFusion debe ser none, raw o merge");
    }

    NS_ABORT_MSG_IF(nodesPerCluster < 2, "nodesPerCluster debe ser >= 2 (cabeza + dueño)");
    NS_ABORT_MSG_IF(ownerCluster >= kClusters.size(), "ownerCluster fuera de rango");
    NS_ABORT_MSG_IF(!collar::MetodoDesdeTexto(estMethod, g_cfg.method),
                    "estMethod debe ser cercano, centroide o trilateracion");
    g_cfg.methodName = estMethod;
    NS_ABORT_MSG_IF(estimator != "cpp" && estimator != "python", "estimator debe ser cpp o python");
    if (estimator == "python")
    {
#ifdef COLLAR_CON_NS3AI
        // Python crea la memoria compartida y luego lanza este programa (analisis/ns3ai_estimador.py).
        auto interface = Ns3AiMsgInterface::Get();
        interface->SetIsMemoryCreator(false);
        interface->SetUseVector(false);
        interface->SetHandleFinish(true);
        interface->SetTrialName(ns3aiTrial);
        g_ai = interface->GetInterface<collar::Peticion, collar::Respuesta>();
#else
        NS_ABORT_MSG("--estimator=python necesita ns3-ai, y este ns-3 se compiló sin el módulo ai "
                     "(ver INSTALACION.md)");
#endif
    }
    g_cfg.p0 = collarTxDbm - refLossDb;
    g_cfg.pathLossExp = pathLossExp;
    g_cfg.estWindow = Seconds(estWindow);
    g_cfg.estInterval = Seconds(estInterval);
    g_cfg.queryInterval = Seconds(queryInterval);
    g_cfg.reportJitterMs = reportJitterMs;

    SystemPath::MakeDirectories(outDir);
    int64_t stream = 1000; // índice de flujos aleatorios; cada modelo toma los suyos

    // ------------------------------------------------------------ nodos
    NodeContainer observers;
    std::vector<NodeContainer> clusterNodes(kClusters.size());
    for (uint32_t c = 0; c < kClusters.size(); ++c)
    {
        clusterNodes[c].Create(nodesPerCluster);
        observers.Add(clusterNodes[c]);
    }
    for (const auto& z : kZones)
    {
        g_zoneHeads.push_back(clusterNodes[z.headCluster].Get(0)->GetId());
    }
    uint32_t ownerId = clusterNodes[ownerCluster].Get(1)->GetId();
    for (uint32_t c = 0; c < kClusters.size(); ++c)
    {
        for (uint32_t k = 0; k < nodesPerCluster; ++k)
        {
            uint32_t id = clusterNodes[c].Get(k)->GetId();
            uint32_t zone = kClusters[c].zone;
            g_nodeInfo[id] = {"observador",
                              int(c),
                              int(zone),
                              k == 0,
                              id == g_zoneHeads[zone],
                              id == ownerId};
            g_app[id].clusterHead = clusterNodes[c].Get(0)->GetId();
            g_app[id].zoneHead = g_zoneHeads[zone];
        }
    }
    NodeContainer collars;
    collars.Create(1);
    g_nodeInfo[collars.Get(0)->GetId()] = {"collar", -1, -1, false, false, false};
    NodeContainer allNodes(observers, collars);

    // ------------------------------------------------------------ movilidad jerárquica
    std::vector<Ptr<MobilityModel>> zoneMm;
    for (const auto& z : kZones)
    {
        Ptr<ConstantPositionMobilityModel> m = CreateObject<ConstantPositionMobilityModel>();
        m->SetPosition(z.origin);
        zoneMm.push_back(m);
    }

    Ptr<UniformRandomVariable> offset = CreateObject<UniformRandomVariable>();
    offset->SetStream(stream++);

    for (uint32_t c = 0; c < kClusters.size(); ++c)
    {
        const ClusterSpec& spec = kClusters[c];

        // Nivel 1: referencia del clúster, relativa a su zona.
        Ptr<MobilityModel> local;
        if (spec.kind == ClusterKind::BUS && clusterMobility)
        {
            local = MakeBusRoute(spec.routeStart, busSpeed, busDwell, simTime);
        }
        else if (spec.kind == ClusterKind::GROUP && clusterMobility)
        {
            Ptr<RandomRectanglePositionAllocator> dest =
                CreateObject<RandomRectanglePositionAllocator>();
            Ptr<UniformRandomVariable> dx = CreateObject<UniformRandomVariable>();
            dx->SetAttribute("Min", DoubleValue(20.0));
            dx->SetAttribute("Max", DoubleValue(kZoneWidth - 20.0));
            Ptr<UniformRandomVariable> dy = CreateObject<UniformRandomVariable>();
            dy->SetAttribute("Min", DoubleValue(20.0));
            dy->SetAttribute("Max", DoubleValue(kZoneHeight - 20.0));
            dest->SetX(dx);
            dest->SetY(dy);
            stream += dest->AssignStreams(stream);

            Ptr<RandomWaypointMobilityModel> rwp = CreateObject<RandomWaypointMobilityModel>();
            std::ostringstream speed;
            speed << "ns3::UniformRandomVariable[Min=" << groupSpeedMin << "|Max=" << groupSpeedMax
                  << "]";
            std::ostringstream pause;
            pause << "ns3::UniformRandomVariable[Min=0|Max=" << groupPause << "]";
            rwp->SetAttribute("Speed", StringValue(speed.str()));
            rwp->SetAttribute("Pause", StringValue(pause.str()));
            rwp->SetAttribute("PositionAllocator", PointerValue(dest));
            stream += rwp->AssignStreams(stream);
            rwp->SetPosition(spec.home);
            local = rwp;
        }
        else
        {
            Ptr<ConstantPositionMobilityModel> fixed = CreateObject<ConstantPositionMobilityModel>();
            fixed->SetPosition(spec.kind == ClusterKind::BUS ? kBusRoute[spec.routeStart]
                                                             : spec.home);
            local = fixed;
        }

        Ptr<HierarchicalMobilityModel> clusterRef = CreateObject<HierarchicalMobilityModel>();
        clusterRef->SetParent(zoneMm[spec.zone]); // padre primero: el hijo queda en coordenadas relativas
        clusterRef->SetChild(local);

        // Nodos: posición = referencia del clúster + desplazamiento propio.
        for (uint32_t k = 0; k < nodesPerCluster; ++k)
        {
            Vector start(offset->GetValue(-spec.halfX, spec.halfX),
                         offset->GetValue(-spec.halfY, spec.halfY),
                         0);
            Ptr<MobilityModel> member;
            if (nodeMobility)
            {
                Ptr<RandomWalk2dMobilityModel> walk = CreateObject<RandomWalk2dMobilityModel>();
                walk->SetAttribute(
                    "Bounds",
                    RectangleValue(Rectangle(-spec.halfX, spec.halfX, -spec.halfY, spec.halfY)));
                std::ostringstream speed;
                speed << "ns3::ConstantRandomVariable[Constant=" << spec.nodeSpeed << "]";
                walk->SetAttribute("Speed", StringValue(speed.str()));
                walk->SetAttribute("Time", TimeValue(Seconds(5.0)));
                walk->SetAttribute("Mode", StringValue("Time"));
                stream += walk->AssignStreams(stream);
                walk->SetPosition(start);
                member = walk;
            }
            else
            {
                Ptr<ConstantPositionMobilityModel> fixed =
                    CreateObject<ConstantPositionMobilityModel>();
                fixed->SetPosition(start);
                member = fixed;
            }
            Ptr<HierarchicalMobilityModel> h = CreateObject<HierarchicalMobilityModel>();
            h->SetParent(clusterRef);
            h->SetChild(member);
            clusterNodes[c].Get(k)->AggregateObject(h);
        }
    }

    // Perro perdido: no pertenece a ningún clúster.
    if (dogMobility)
    {
        Ptr<RandomWalk2dMobilityModel> walk = CreateObject<RandomWalk2dMobilityModel>();
        walk->SetAttribute("Bounds", RectangleValue(Rectangle(0, kZoneWidth, 0, 2 * kZoneHeight)));
        std::ostringstream speed;
        speed << "ns3::ConstantRandomVariable[Constant=" << dogSpeed << "]";
        walk->SetAttribute("Speed", StringValue(speed.str()));
        walk->SetAttribute("Time", TimeValue(Seconds(20.0)));
        walk->SetAttribute("Mode", StringValue("Time"));
        stream += walk->AssignStreams(stream);
        walk->SetPosition(Vector(dogX, dogY, 0));
        g_dogMm = walk;
    }
    else
    {
        g_dogMm = CreateObject<ConstantPositionMobilityModel>();
        g_dogMm->SetPosition(Vector(dogX, dogY, 0));
    }
    collars.Get(0)->AggregateObject(g_dogMm);

    // ------------------------------------------------------------ Wi-Fi ad hoc 802.11b
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                               "Exponent",
                               DoubleValue(pathLossExp),
                               "ReferenceLoss",
                               DoubleValue(refLossDb));
    if (shadowSigmaDb > 0)
    {
        std::ostringstream shadow;
        shadow << "ns3::NormalRandomVariable[Mean=0|Variance=" << shadowSigmaDb * shadowSigmaDb
               << "]";
        channel.AddPropagationLoss("ns3::RandomPropagationLossModel",
                                   "Variable",
                                   StringValue(shadow.str()));
    }
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.Set("RxSensitivity", DoubleValue(rxSensitivityDbm));
    phy.SetPreambleDetectionModel("ns3::ThresholdPreambleDetectionModel",
                                  "MinimumRssi",
                                  DoubleValue(rxSensitivityDbm));

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(phoneDataMode),
                                 "ControlMode",
                                 StringValue("DsssRate1Mbps"));
    phy.Set("TxPowerStart", DoubleValue(phoneTxDbm));
    phy.Set("TxPowerEnd", DoubleValue(phoneTxDbm));
    NetDeviceContainer observerDevs = wifi.Install(phy, mac, observers);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("DsssRate1Mbps"),
                                 "ControlMode",
                                 StringValue("DsssRate1Mbps"));
    phy.Set("TxPowerStart", DoubleValue(collarTxDbm));
    phy.Set("TxPowerEnd", DoubleValue(collarTxDbm));
    NetDeviceContainer collarDevs = wifi.Install(phy, mac, collars);

    stream += wifi.AssignStreams(NetDeviceContainer(observerDevs, collarDevs), stream);
    stream += wifiChannel->AssignStreams(stream); // sombreado

    for (uint32_t i = 0; i < collarDevs.GetN(); ++i)
    {
        Mac48Address addr = Mac48Address::ConvertFrom(collarDevs.Get(i)->GetAddress());
        g_collarByMac[addr] = collars.Get(i)->GetId();
    }

    // ------------------------------------------------------------ IP y enrutamiento
    // Con la cola ARP de 3 paquetes que trae ns-3, las ráfagas de relevos (una por beacon) se pierden
    // cada vez que el enrutamiento cambia de siguiente salto: con AODV caían ~40 % de los relevos.
    Config::SetDefault("ns3::ArpCache::PendingQueueSize", UintegerValue(arpQueue));

    // Los celulares enrutan; el collar no (solo emite su beacon de un salto).
    InternetStackHelper phoneStack;
    if (routing == "aodv")
    {
        AodvHelper aodv;
        aodv.Set("EnableBroadcast", BooleanValue(false)); // no reenviar el beacon del collar
        phoneStack.SetRoutingHelper(aodv);
        phoneStack.Install(observers);
        stream += aodv.AssignStreams(observers, stream);
    }
    else if (routing == "olsr")
    {
        OlsrHelper olsr;
        phoneStack.SetRoutingHelper(olsr);
        phoneStack.Install(observers);
        stream += olsr.AssignStreams(observers, stream);
    }
    else if (routing == "dsdv")
    {
        DsdvHelper dsdv;
        phoneStack.SetRoutingHelper(dsdv);
        phoneStack.Install(observers);
    }
    else
    {
        NS_ABORT_MSG("routing debe ser aodv, olsr o dsdv");
    }
    InternetStackHelper collarStack;
    collarStack.Install(collars);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    ipv4.Assign(NetDeviceContainer(observerDevs, collarDevs));

    // ------------------------------------------------------------ aplicaciones
    g_appJitter = CreateObject<UniformRandomVariable>();
    g_appJitter->SetStream(stream++);
    for (auto it = observers.Begin(); it != observers.End(); ++it)
    {
        uint32_t id = (*it)->GetId();
        NodeApp& app = g_app[id];
        app.address = (*it)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
        app.socket = Socket::CreateSocket(*it, UdpSocketFactory::GetTypeId());
        app.socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), kAppPort));
        app.socket->SetRecvCallback(MakeCallback(&ReceiveMsg));
    }
    for (uint32_t zh : g_zoneHeads)
    {
        Simulator::Schedule(g_cfg.estInterval, &EstimateTick, zh);
    }
    Simulator::Schedule(Seconds(queryStart), &QueryTick, ownerId);

    Ptr<UniformRandomVariable> jitter = CreateObject<UniformRandomVariable>();
    jitter->SetStream(stream++);
    for (uint32_t i = 0; i < collars.GetN(); ++i)
    {
        Ptr<Socket> s = Socket::CreateSocket(collars.Get(i), UdpSocketFactory::GetTypeId());
        s->SetAllowBroadcast(true);
        s->Connect(InetSocketAddress(Ipv4Address("255.255.255.255"), kBeaconPort));
        Simulator::Schedule(Seconds(1.0) + MilliSeconds(jitter->GetValue(0, 100)),
                            &SendBeacon,
                            s,
                            Seconds(beaconInterval),
                            jitter,
                            beaconSize);
    }

    // ------------------------------------------------------------ registros
    g_gpsNoise = CreateObject<NormalRandomVariable>();
    g_gpsNoise->SetAttribute("Mean", DoubleValue(0.0));
    g_gpsNoise->SetAttribute("Variance", DoubleValue(gpsSigma * gpsSigma));
    g_gpsNoise->SetStream(stream++);

    g_rssiOut.open(outDir + "/rssi.csv");
    g_rssiOut << "t,observador,cluster,zona,obs_x,obs_y,gps_x,gps_y,collar,collar_x,collar_y,"
                 "rssi_dbm,dist_real\n";
    g_posOut.open(outDir + "/posiciones.csv");
    g_posOut << "t,nodo,rol,cluster,zona,x,y\n";
    g_estOut.open(outDir + "/estimaciones_red.csv");
    g_estOut << "t,zona,cabeza_zona,metodo,valida,x,y,verdad_x,verdad_y,error_m,lecturas,"
                "observadores\n";

    {
        std::ofstream nodesOut(outDir + "/nodos.csv");
        nodesOut << "nodo,rol,cluster,nombre_cluster,zona,nombre_zona,cabeza_cluster,cabeza_zona,"
                    "dueno\n";
        for (const auto& [id, info] : g_nodeInfo)
        {
            nodesOut << id << "," << info.role << "," << info.cluster << ","
                     << (info.cluster >= 0 ? kClusters[info.cluster].name : "") << ","
                     << info.zone << "," << (info.zone >= 0 ? kZones[info.zone].name : "") << ","
                     << info.clusterHead << "," << info.zoneHead << "," << info.owner << "\n";
        }
    }

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
                    MakeCallback(&SnifferRx));
    Simulator::Schedule(Seconds(0), &LogPositions, allNodes, Seconds(posLogInterval));

    if (netanim)
    {
        for (const auto& zone : kZones)
        {
            Ptr<Node> marker = CreateObject<Node>();
            Ptr<ConstantPositionMobilityModel> mobility =
                CreateObject<ConstantPositionMobilityModel>();
            mobility->SetPosition(
                Vector(kZoneWidth / 2, zone.origin.y + kZoneHeight / 2, 0));
            marker->AggregateObject(mobility);
            g_estimationMarkerNodes.push_back(marker);
            g_estimationMarkerModels.push_back(mobility);
            g_estimationMarkerVisible.push_back(false);
        }
    }

    std::unique_ptr<AnimationInterface> anim;
    if (netanim)
    {
        anim = std::make_unique<AnimationInterface>(outDir + "/netanim.xml");
        g_anim = anim.get();
        anim->SetMobilityPollInterval(Seconds(0.5));
        anim->SkipPacketTracing();
        for (uint32_t c = 0; c < kClusters.size(); ++c)
        {
            for (uint32_t k = 0; k < clusterNodes[c].GetN(); ++k)
            {
                Ptr<Node> n = clusterNodes[c].Get(k);
                const NodeInfo& info = g_nodeInfo[n->GetId()];
                std::string label = " ";
                if (info.zoneHead)
                {
                    label = info.zone == 0 ? "HZ-S" : "HZ-N";
                }
                else if (info.owner)
                {
                    label = "D";
                }
                anim->UpdateNodeColor(n, kClusters[c].r, kClusters[c].g, kClusters[c].b);
                anim->UpdateNodeDescription(n, label);
                double size = info.zoneHead ? 6.0 : (info.clusterHead || info.owner ? 4.5 : 3.0);
                anim->UpdateNodeSize(n, size, size);
            }
        }
        anim->UpdateNodeColor(collars.Get(0), 230, 20, 20);
        anim->UpdateNodeDescription(collars.Get(0), "REAL");
        anim->UpdateNodeSize(collars.Get(0), 6.0, 6.0);
        for (std::size_t i = 0; i < g_estimationMarkerNodes.size(); ++i)
        {
            const auto& marker = g_estimationMarkerNodes[i];
            const auto& zone = kZones[i];
            anim->UpdateNodeColor(marker, 130, 130, 130);
            anim->UpdateNodeDescription(marker,
                                        zone.name == "sur" ? "EST-S sin dato" : "EST-N sin dato");
            anim->UpdateNodeSize(marker, 8.0, 8.0);
        }
    }

    // ------------------------------------------------------------ correr
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    // Con --estimator=python no se llama CppSetFinished() aquí: el destructor de la interfaz ns3-ai lo
    // hace al salir del programa, y una segunda llamada se queda esperando a Python para siempre.
    WriteResults(outDir);

    {
        std::ofstream params(outDir + "/parametros.txt");
        params << "RngRun=" << RngSeedManager::GetRun() << "\n"
               << "RngSeed=" << RngSeedManager::GetSeed() << "\n"
               << "clusterMobility=" << clusterMobility << "\n"
               << "nodeMobility=" << nodeMobility << "\n"
               << "nodesPerCluster=" << nodesPerCluster << "\n"
               << "simTime=" << simTime << "\n"
               << "busSpeed=" << busSpeed << "\n"
               << "busDwell=" << busDwell << "\n"
               << "groupSpeedMin=" << groupSpeedMin << "\n"
               << "groupSpeedMax=" << groupSpeedMax << "\n"
               << "groupPause=" << groupPause << "\n"
               << "dogMobility=" << dogMobility << "\n"
               << "dogX=" << dogX << "\n"
               << "dogY=" << dogY << "\n"
               << "dogSpeed=" << dogSpeed << "\n"
               << "beaconInterval=" << beaconInterval << "\n"
               << "beaconSize=" << beaconSize << "\n"
               << "collarTxDbm=" << collarTxDbm << "\n"
               << "phoneTxDbm=" << phoneTxDbm << "\n"
               << "rxSensitivityDbm=" << rxSensitivityDbm << "\n"
               << "pathLossExp=" << pathLossExp << "\n"
               << "refLossDb=" << refLossDb << "\n"
               << "shadowSigmaDb=" << shadowSigmaDb << "\n"
               << "gpsSigma=" << gpsSigma << "\n"
               << "estWindow=" << estWindow << "\n"
               << "estInterval=" << estInterval << "\n"
               << "queryInterval=" << queryInterval << "\n"
               << "queryStart=" << queryStart << "\n"
               << "ownerCluster=" << ownerCluster << "\n"
               << "reportJitterMs=" << reportJitterMs << "\n"
               << "arpQueue=" << arpQueue << "\n"
               << "beaconsSent=" << g_beaconsSent << "\n"
               << "rssiRecords=" << g_reports.size() << "\n"
               << "mensajes=" << g_msgs.size() << "\n"
               << "consultas=" << g_queries.size() << "\n";
        // Texto: se guardan aparte para que localizar.py pueda leer los numéricos con float().
        std::ofstream texto(outDir + "/parametros_texto.txt");
        texto << "routing=" << routing << "\n"
              << "phoneDataMode=" << phoneDataMode << "\n"
              << "estMethod=" << estMethod << "\n"
              << "estimator=" << estimator << "\n"
              << "zoneFusion=" << zoneFusion << "\n";
        for (const auto& f : confFiles)
        {
            texto << "conf=" << f << "\n";
        }
        texto
              << "ns3aiTrial=" << ns3aiTrial << "\n"
              << "ventanasTruncadas=" << g_aiTruncated << "\n";
    }
    std::cout << "beacons: " << g_beaconsSent << ", lecturas: " << g_reports.size()
              << ", mensajes: " << g_msgs.size() << ", consultas: " << g_queries.size()
              << ", salida: " << outDir << std::endl;

    Simulator::Destroy();
    return 0;
}

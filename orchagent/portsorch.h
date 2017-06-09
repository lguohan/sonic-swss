#ifndef SWSS_PORTSORCH_H
#define SWSS_PORTSORCH_H

#include <map>
#include <boost/bimap.hpp>

#include "orch.h"
#include "port.h"
#include "observer.h"
#include "macaddress.h"

#include <map>

#define FCS_LEN 4
#define VLAN_TAG_LEN 4

static const map<sai_port_oper_status_t, string> oper_status_strings =
{
    { SAI_PORT_OPER_STATUS_UNKNOWN,     "unknown" },
    { SAI_PORT_OPER_STATUS_UP,          "up" },
    { SAI_PORT_OPER_STATUS_DOWN,        "down" },
    { SAI_PORT_OPER_STATUS_TESTING,     "testing" },
    { SAI_PORT_OPER_STATUS_NOT_PRESENT, "not present" }
};

struct LagMemberUpdate
{
    Port lag;
    Port member;
    bool add;
};

struct VlanMemberUpdate
{
    Port vlan;
    Port member;
    bool add;
};

class PortsOrch : public Orch, public Subject
{
public:
    PortsOrch(DBConnector *db, vector<string> tableNames);

    bool isInitDone();

    map<string, Port>& getAllPorts();
    bool getBridgePort(sai_object_id_t id, Port &port);
    bool getPort(string alias, Port &port);
    bool getPort(sai_object_id_t id, Port &port);
    void setPort(string alias, Port port);
    sai_object_id_t getCpuPort();

    bool setHostIntfsOperStatus(sai_object_id_t id, bool up);
    void updateDbPortOperStatus(sai_object_id_t id, sai_port_oper_status_t status);
    bool removeBridgePort(Port port);
private:
    unique_ptr<Table> m_counterTable;
    unique_ptr<Table> m_portTable;

    bool m_initDone = false;
    sai_object_id_t m_cpuPort;
    sai_object_id_t m_default1QBridge;
    sai_object_id_t m_defaultVlan;

    sai_uint32_t m_portCount;
    map<set<int>, sai_object_id_t> m_portListLaneMap;
    boost::bimap<sai_object_id_t, sai_object_id_t> m_bridgePort; // port id -> bridge port id
    map<sai_object_id_t, sai_object_id_t> m_bridgePortVlanMember; // bridge port id -> vlan member id
    map<string, Port> m_portList;

    void doTask(Consumer &consumer);
    void doPortTask(Consumer &consumer);
    void doVlanTask(Consumer &consumer);
    void doVlanMemberTask(Consumer &consumer);
    void doLagTask(Consumer &consumer);
    void doLagMemberTask(Consumer &consumer);

    bool initializePort(Port &port);
    void initializePriorityGroups(Port &port);
    void initializeQueues(Port &port);

    bool addHostIntfs(sai_object_id_t router_intfs_id, string alias, sai_object_id_t &host_intfs_id);

    bool addVlan(string vlan);
    bool removeVlan(Port vlan);
    bool addVlanMember(Port vlan, Port port, string& tagging_mode);
    bool removeVlanMember(Port vlan, Port port);

    bool addLag(string lag);
    bool removeLag(Port lag);
    bool addLagMember(Port lag, Port port);
    bool removeLagMember(Port lag, Port port);

    bool setPortAdminStatus(sai_object_id_t id, bool up);
    bool setPortMtu(sai_object_id_t id, sai_uint32_t mtu);
};
#endif /* SWSS_PORTSORCH_H */


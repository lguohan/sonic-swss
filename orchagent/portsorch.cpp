#include "portsorch.h"

#include <cassert>
#include <fstream>
#include <sstream>
#include <set>

#include <netinet/if_ether.h>
#include "net/if.h"

#include "logger.h"
#include "schema.h"

extern sai_switch_api_t *sai_switch_api;
extern sai_bridge_api_t *sai_bridge_api;
extern sai_port_api_t *sai_port_api;
extern sai_vlan_api_t *sai_vlan_api;
extern sai_lag_api_t *sai_lag_api;
extern sai_hostif_api_t* sai_hostif_api;
extern sai_acl_api_t* sai_acl_api;
extern sai_object_id_t gSwitchId;

#define VLAN_PREFIX         "Vlan"
#define DEFAULT_VLAN_ID     1

PortsOrch::PortsOrch(DBConnector *db, vector<string> tableNames) :
        Orch(db, tableNames)
{
    SWSS_LOG_ENTER();

    /* Initialize counter table */
    DBConnector *counter_db(new DBConnector(COUNTERS_DB, DBConnector::DEFAULT_UNIXSOCKET, 0));
    m_counterTable = unique_ptr<Table>(new Table(counter_db, COUNTERS_PORT_NAME_MAP));

    /* Initialize port table */
    m_portTable = unique_ptr<Table>(new Table(m_db, APP_PORT_TABLE_NAME));

    uint32_t i, j;
    sai_status_t status;
    sai_attribute_t attr;

    /* Get CPU port */
    attr.id = SAI_SWITCH_ATTR_CPU_PORT;

    status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get CPU port");
        throw "PortsOrch initialization failure";
    }

    m_cpuPort = attr.value.oid;

    /* Get port number */
    attr.id = SAI_SWITCH_ATTR_PORT_NUMBER;

    status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get port number");
        throw "PortsOrch initialization failure";
    }

    m_portCount = attr.value.u32;

    SWSS_LOG_NOTICE("Get port number : %d", m_portCount);

    /* Get port list */
    vector<sai_object_id_t> port_list;
    port_list.resize(m_portCount);

    attr.id = SAI_SWITCH_ATTR_PORT_LIST;
    attr.value.objlist.count = port_list.size();
    attr.value.objlist.list = port_list.data();

    status = sai_switch_api->get_switch_attribute(gSwitchId, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get port list");
        throw "PortsOrch initialization failure";
    }

    /* Get port hardware lane info */
    for (i = 0; i < m_portCount; i++)
    {
        sai_uint32_t lanes[4];
        attr.id = SAI_PORT_ATTR_HW_LANE_LIST;
        attr.value.u32list.count = 4;
        attr.value.u32list.list = lanes;

        status = sai_port_api->get_port_attribute(port_list[i], 1, &attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to get hardware lane list pid:%lx", port_list[i]);
            throw "PortsOrch initialization failure";
        }

        set<int> tmp_lane_set;
        for (j = 0; j < attr.value.u32list.count; j++)
            tmp_lane_set.insert(attr.value.u32list.list[j]);

        string tmp_lane_str = "";
        for (auto s : tmp_lane_set)
        {
            tmp_lane_str += to_string(s) + " ";
        }
        tmp_lane_str = tmp_lane_str.substr(0, tmp_lane_str.size()-1);

        SWSS_LOG_NOTICE("Get port with lanes pid:%lx lanes:%s", port_list[i], tmp_lane_str.c_str());
        m_portListLaneMap[tmp_lane_set] = port_list[i];
    }

    // Get default bridge id and VLAN id
    vector<sai_attribute_t> switch_attrs;
    sai_attribute_t switch_attr;
    switch_attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;
    switch_attrs.push_back(switch_attr);
    switch_attr.id = SAI_SWITCH_ATTR_DEFAULT_VLAN_ID;
    switch_attrs.push_back(switch_attr);

    status = sai_switch_api->get_switch_attribute(gSwitchId, switch_attrs.size(), switch_attrs.data());
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get default 1Q_BRIDGE: %d", status);
        throw "PortsOrch initialization failure";
    }
    m_default1QBridge = switch_attrs[0].value.oid;
    m_defaultVlan = switch_attrs[1].value.oid;

    // Get all bridge ports in the default 1Q bridge
    sai_attribute_t bridge_attr;
    bridge_attr.id = SAI_BRIDGE_ATTR_PORT_LIST;
    vector<sai_object_id_t> bp_list(100);
    bridge_attr.value.objlist.list = bp_list.data();
    bridge_attr.value.objlist.count = bp_list.size();
    status = sai_bridge_api->get_bridge_attribute(m_default1QBridge, 1, &bridge_attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get bridge ports in default 1Q_BRIDGE: %d", status);
        throw "PortsOrch initialization failure";
    }

    // For every bridge port from default bridge ...
    for (i = 0; i < bridge_attr.value.objlist.count; ++i)
    {
        sai_object_id_t bridge_port_id = bridge_attr.value.objlist.list[i];

        // Get the bridge port attributes
        sai_attribute_t bport_attr;
        bport_attr.id = SAI_BRIDGE_PORT_ATTR_TYPE;
        status = sai_bridge_api->get_bridge_port_attribute(bridge_port_id, 1, &bport_attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to get bridge port %lx type attribute: %d", bridge_port_id, status);
            throw "PortsOrch initialization failure";
        }

        if (bport_attr.value.s32 != SAI_BRIDGE_PORT_TYPE_PORT) continue;

        // Set the learning_mode
        bport_attr.id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;
        bport_attr.value.s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW;
        status = sai_bridge_api->set_bridge_port_attribute(bridge_port_id, &bport_attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to set bridge port %lx learning_mode attribute: %d", bridge_port_id, status);
            throw "PortsOrch initialization failure";
        }

        // Get the port id
        bport_attr.id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
        status = sai_bridge_api->get_bridge_port_attribute(bridge_port_id, 1, &bport_attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to get bridge port %lx port_id attribute: %d", bridge_port_id, status);
            throw "PortsOrch initialization failure";
        }
        sai_object_id_t port_id = bport_attr.value.oid;
        m_bridgePort.insert({port_id, bridge_port_id});
    }

    // Get all the bridge ports in the dfault VLAN
    sai_attribute_t vlan_attr;
    vlan_attr.id = SAI_VLAN_ATTR_MEMBER_LIST;
    vector<sai_object_id_t> vm_list(100);
    vlan_attr.value.objlist.list = vm_list.data();
    vlan_attr.value.objlist.count = vm_list.size();
    status = sai_vlan_api->get_vlan_attribute(m_defaultVlan, 1, &vlan_attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get VLAN members in default VLAN: %d", status);
        throw "PortsOrch initialization failure";
    }

    // For every VLAN member in the default VLAN ...
    for (i = 0; i < vlan_attr.value.objlist.count; ++i)
    {
        sai_object_id_t vlan_member_id = vlan_attr.value.objlist.list[i];

        // Get the VLAN member attribute
        sai_attribute_t vm_attr;
        vm_attr.id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
        status = sai_vlan_api->get_vlan_member_attribute(vlan_member_id, 1, &vm_attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to vlan member %lx bridge port attribute: %d", vlan_member_id, status);
            throw "PortsOrch initialization failure";
        }
        sai_object_id_t bridge_port_id = vm_attr.value.oid;
        m_bridgePortVlanMember[bridge_port_id] = vlan_member_id;
    }
}

bool PortsOrch::isInitDone()
{
    return m_initDone;
}

map<string, Port>& PortsOrch::getAllPorts()
{
    return m_portList;
}

bool PortsOrch::getBridgePort(sai_object_id_t bridge_port_id, Port &port)
{
    if (m_bridgePort.right.find(bridge_port_id) == m_bridgePort.right.end())
        return false;

    sai_object_id_t port_id = m_bridgePort.right.at(bridge_port_id);

    // TODO: optimize by dictionary query
    for (auto& pair: m_portList)
    {
        auto& port1 = pair.second;
        if (port1.m_port_id == port_id)
        {
            port = port1;
            return true;
        }
    }

    return false;
}

bool PortsOrch::getPort(string alias, Port &p)
{
    SWSS_LOG_ENTER();

    if (m_portList.find(alias) == m_portList.end())
        return false;
    p = m_portList[alias];
    return true;
}

bool PortsOrch::getPort(sai_object_id_t id, Port &port)
{
    SWSS_LOG_ENTER();

    for (const auto& portIter: m_portList)
    {
        switch (portIter.second.m_type)
        {
        case Port::PHY:
            if(portIter.second.m_port_id == id)
            {
                port = portIter.second;
                return true;
            }
            break;
        case Port::LAG:
            if(portIter.second.m_lag_id == id)
            {
                port = portIter.second;
                return true;
            }
            break;
        default:
            continue;
        }
    }

    return false;
}

void PortsOrch::setPort(string alias, Port p)
{
    m_portList[alias] = p;
}

sai_object_id_t PortsOrch::getCpuPort()
{
    return m_cpuPort;
}

bool PortsOrch::setPortAdminStatus(sai_object_id_t id, bool up)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_ADMIN_STATE;
    attr.value.booldata = up;

    sai_status_t status = sai_port_api->set_port_attribute(id, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to set admin status %s to port pid:%lx",
                       up ? "UP" : "DOWN", id);
        return false;
    }
    SWSS_LOG_INFO("Set admin status %s to port pid:%lx",
                    up ? "UP" : "DOWN", id);
    return true;
}

bool PortsOrch::setPortMtu(sai_object_id_t id, sai_uint32_t mtu)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_MTU;
    /* mtu + 14 + 4 + 4 = 22 bytes */
    attr.value.u32 = mtu + sizeof(struct ether_header) + FCS_LEN + VLAN_TAG_LEN;

    sai_status_t status = sai_port_api->set_port_attribute(id, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to set MTU %u to port pid:%lx", attr.value.u32, id);
        return false;
    }
    SWSS_LOG_INFO("Set MTU %u to port pid:%lx", attr.value.u32, id);
    return true;
}

bool PortsOrch::setHostIntfsOperStatus(sai_object_id_t port_id, bool up)
{
    SWSS_LOG_ENTER();

    for (auto it = m_portList.begin(); it != m_portList.end(); it++)
    {
        if (it->second.m_port_id == port_id)
        {
            sai_attribute_t attr;
            attr.id = SAI_HOSTIF_ATTR_OPER_STATUS;
            attr.value.booldata = up;

            sai_status_t status = sai_hostif_api->set_hostif_attribute(it->second.m_hif_id, &attr);
            if (status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_WARN("Failed to set operation status %s to host interface %s",
                              up ? "UP" : "DOWN", it->second.m_alias.c_str());
                return false;
            }
            SWSS_LOG_NOTICE("Set operation status %s to host interface %s",
                            up ? "UP" : "DOWN", it->second.m_alias.c_str());
            return true;
        }
    }
    return false;
}

void PortsOrch::updateDbPortOperStatus(sai_object_id_t id, sai_port_oper_status_t status)
{
    SWSS_LOG_ENTER();

    for (auto it = m_portList.begin(); it != m_portList.end(); it++)
    {
        if (it->second.m_port_id == id)
        {
            vector<FieldValueTuple> vector;
            FieldValueTuple tuple("oper_status", oper_status_strings.at(status));
            vector.push_back(tuple);
            m_portTable->set(it->first, vector);
        }
    }
}

void PortsOrch::doPortTask(Consumer &consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        auto &t = it->second;

        string alias = kfvKey(t);
        string op = kfvOp(t);

        /* Get notification from application */
        /* portsyncd application:
         * When portsorch receives 'ConfigDone' message, it indicates port initialization
         * procedure is done. Before port initialization procedure, none of other tasks
         * are executed.
         */
        if (alias == "ConfigDone")
        {
            /* portsyncd restarting case:
             * When portsyncd restarts, duplicate notifications may be received.
             */
            if (!m_initDone)
            {
                m_initDone = true;
                SWSS_LOG_INFO("Get ConfigDone notification from portsyncd.");
            }

            it = consumer.m_toSync.erase(it);
            return;
        }

        if (op == "SET")
        {
            set<int> lane_set;
            string admin_status;
            uint32_t mtu = 0;

            for (auto i : kfvFieldsValues(t))
            {
                /* Get lane information of a physical port and initialize the port */
                if (fvField(i) == "lanes")
                {
                    string lane_str;
                    istringstream iss(fvValue(i));

                    while (getline(iss, lane_str, ','))
                    {
                        int lane = stoi(lane_str);
                        lane_set.insert(lane);
                    }

                }

                /* Set port admin status */
                if (fvField(i) == "admin_status")
                    admin_status = fvValue(i);

                /* Set port mtu */
                if (fvField(i) == "mtu")
                    mtu = stoul(fvValue(i));
            }

            if (lane_set.size())
            {
                /* Determine if the lane combination exists in switch */
                if (m_portListLaneMap.find(lane_set) !=
                    m_portListLaneMap.end())
                {
                    sai_object_id_t id = m_portListLaneMap[lane_set];

                    /* Determin if the port has already been initialized before */
                    if (m_portList.find(alias) != m_portList.end() && m_portList[alias].m_port_id == id)
                    {
                        SWSS_LOG_INFO("Port has already been initialized before alias:%s", alias.c_str());
                    }
                    else
                    {
                        Port p(alias, Port::PHY);

                        p.m_index = m_portList.size(); // TODO: Assume no deletion of physical port
                        p.m_port_id = id;

                        /* Initialize the port and create router interface and host interface */
                        if (initializePort(p))
                        {
                            /* Add port to port list */
                            m_portList[alias] = p;
                            /* Add port name map to counter table */
                            std::stringstream ss;
                            ss << hex << p.m_port_id;
                            FieldValueTuple tuple(p.m_alias, ss.str());
                            vector<FieldValueTuple> vector;
                            vector.push_back(tuple);
                            m_counterTable->set("", vector);

                            SWSS_LOG_NOTICE("Initialized port %s", alias.c_str());
                        }
                        else
                            SWSS_LOG_ERROR("Failed to initialize port %s", alias.c_str());
                    }
                }
                else
                    SWSS_LOG_ERROR("Failed to locate port lane combination alias:%s", alias.c_str());
            }

            if (admin_status != "")
            {
                Port p;
                if (getPort(alias, p))
                {
                    if (setPortAdminStatus(p.m_port_id, admin_status == "up"))
                        SWSS_LOG_NOTICE("Set port %s admin status to %s", alias.c_str(), admin_status.c_str());
                    else
                    {
                        SWSS_LOG_ERROR("Failed to set port %s admin status to %s", alias.c_str(), admin_status.c_str());
                        it++;
                        continue;
                    }
                }
                else
                    SWSS_LOG_ERROR("Failed to get port id by alias:%s", alias.c_str());
            }

            if (mtu != 0)
            {
                Port p;
                if (getPort(alias, p))
                {
                    if (setPortMtu(p.m_port_id, mtu))
                        SWSS_LOG_NOTICE("Set port %s MTU to %u", alias.c_str(), mtu);
                    else
                    {
                        SWSS_LOG_ERROR("Failed to set port %s MTU to %u", alias.c_str(), mtu);
                        it++;
                        continue;
                    }
                }
                else
                    SWSS_LOG_ERROR("Failed to get port id by alias:%s", alias.c_str());
            }
        }
        else
            SWSS_LOG_ERROR("Unknown operation type %s", op.c_str());

        it = consumer.m_toSync.erase(it);
    }
}

void PortsOrch::doVlanTask(Consumer &consumer)
{
    if (!isInitDone())
        return;

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        auto &t = it->second;

        string key = kfvKey(t);

        /* Ensure the key starts with "Vlan" otherwise ignore */
        if (strncmp(key.c_str(), VLAN_PREFIX, 4))
        {
            SWSS_LOG_ERROR("Invalid key format. No 'Vlan' prefix: %s", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        int vlan_id;
        vlan_id = stoi(key.substr(4)); // FIXME: might raise exception

        string vlan_alias, port_alias;
        vlan_alias = VLAN_PREFIX + to_string(vlan_id);
        string op = kfvOp(t);

        if (op == SET_COMMAND)
        {
            /* Duplicate entry */
            if (m_portList.find(vlan_alias) != m_portList.end())
            {
                it = consumer.m_toSync.erase(it);
                continue;
            }

            if (addVlan(vlan_alias))
                it = consumer.m_toSync.erase(it);
            else
                it++;
        }
        else if (op == DEL_COMMAND)
        {
            Port vlan;
            getPort(vlan_alias, vlan);

            if (removeVlan(vlan))
                it = consumer.m_toSync.erase(it);
            else
                it++;
        }
        else
        {
            SWSS_LOG_ERROR("Unknown operation type %s", op.c_str());
            it = consumer.m_toSync.erase(it);
        }
    }
}

void PortsOrch::doVlanMemberTask(Consumer &consumer)
{
    if (!isInitDone())
        return;

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        auto &t = it->second;

        string key = kfvKey(t);

        /* Ensure the key starts with "Vlan" otherwise ignore */
        if (strncmp(key.c_str(), VLAN_PREFIX, 4))
        {
            SWSS_LOG_ERROR("Invalid key format. No 'Vlan' prefix: %s", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        key = key.substr(4);
        size_t found = key.find(':');
        int vlan_id;
        string vlan_alias, port_alias;
        if (found != string::npos)
        {
            vlan_id = stoi(key.substr(0, found)); // FIXME: might raise exception
            port_alias = key.substr(found+1);
        }
        else
        {
            SWSS_LOG_ERROR("Invalid key format. No member port is presented: %s",
                           kfvKey(t).c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        vlan_alias = VLAN_PREFIX + to_string(vlan_id);
        string op = kfvOp(t);

        assert(m_portList.find(vlan_alias) != m_portList.end());
        Port vlan, port;

        /* When VLAN member is to be created before VLAN is created */
        if (!getPort(vlan_alias, vlan))
        {
            SWSS_LOG_INFO("Failed to locate VLAN %s", vlan_alias.c_str());
            it++;
            continue;
        }

        if (!getPort(port_alias, port))
        {
            SWSS_LOG_ERROR("Failed to locate port %s", port_alias.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        if (op == SET_COMMAND)
        {
            string tagging_mode = "untagged";

            for (auto i : kfvFieldsValues(t))
            {
                if (fvField(i) == "tagging_mode")
                    tagging_mode = fvValue(i);
            }

            if (tagging_mode != "untagged" &&
                tagging_mode != "tagged"   &&
                tagging_mode != "priority_tagged")
            {
                SWSS_LOG_ERROR("Wrong tagging_mode '%s' for key: %s", tagging_mode.c_str(), kfvKey(t).c_str());
                it = consumer.m_toSync.erase(it);
                continue;
            }

            /* Duplicate entry */
            if (vlan.m_members.find(port_alias) != vlan.m_members.end())
            {
                it = consumer.m_toSync.erase(it);
                continue;
            }

            /* Assert the port doesn't belong to any VLAN */
            assert(!port.m_vlan_id && !port.m_vlan_member_id);

            if (addVlanMember(vlan, port, tagging_mode))
                it = consumer.m_toSync.erase(it);
            else
                it++;
        }
        else if (op == DEL_COMMAND)
        {
            if (vlan.m_members.find(port_alias) != vlan.m_members.end())
            {
                /* Assert the port belongs the a VLAN */
                assert(port.m_vlan_id && port.m_vlan_member_id);

                if (removeVlanMember(vlan, port))
                    it = consumer.m_toSync.erase(it);
                else
                    it++;
            }
            else
                /* Cannot locate the VLAN */
                it = consumer.m_toSync.erase(it);
        }
        else
        {
            SWSS_LOG_ERROR("Unknown operation type %s", op.c_str());
            it = consumer.m_toSync.erase(it);
        }
    }
}

void PortsOrch::doLagTask(Consumer &consumer)
{
    if (!isInitDone())
        return;

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        auto &t = it->second;

        string lag_alias = kfvKey(t);
        string op = kfvOp(t);

        if (op == SET_COMMAND)
        {
            /* Duplicate entry */
            if (m_portList.find(lag_alias) != m_portList.end())
            {
                it = consumer.m_toSync.erase(it);
                continue;
            }

            if (addLag(lag_alias))
                it = consumer.m_toSync.erase(it);
            else
                it++;
        }
        else if (op == DEL_COMMAND)
        {
            Port lag;
            /* Cannot locate LAG */
            if (!getPort(lag_alias, lag))
            {
                it = consumer.m_toSync.erase(it);
                continue;
            }

            if (removeLag(lag))
                it = consumer.m_toSync.erase(it);
            else
                it++;
        }
        else
        {
            SWSS_LOG_ERROR("Unknown operation type %s", op.c_str());
            it = consumer.m_toSync.erase(it);
        }
    }
}

void PortsOrch::doLagMemberTask(Consumer &consumer)
{
    if (!isInitDone())
        return;

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        auto &t = it->second;

        /* Retrieve LAG alias and LAG member alias from key */
        string key = kfvKey(t);
        size_t found = key.find(':');
        /* Return if the format of key is wrong */
        if (found == string::npos)
        {
            SWSS_LOG_ERROR("Failed to parse %s", key.c_str());
            return;
        }
        string lag_alias = key.substr(0, found);
        string port_alias = key.substr(found+1);

        string op = kfvOp(t);

        Port lag, port;
        if (!getPort(lag_alias, lag))
        {
            SWSS_LOG_INFO("Failed to locate LAG %s", lag_alias.c_str());
            it++;
            continue;
        }

        if (!getPort(port_alias, port))
        {
            SWSS_LOG_ERROR("Failed to locate port %s", port_alias.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        /* Update a LAG member */
        if (op == SET_COMMAND)
        {
            string status;
            for (auto i : kfvFieldsValues(t))
            {
                if (fvField(i) == "status")
                    status = fvValue(i);
            }

            /* Sync an enabled member */
            if (status == "enabled")
            {
                /* Duplicate entry */
                if (lag.m_members.find(port_alias) != lag.m_members.end())
                {
                    it = consumer.m_toSync.erase(it);
                    continue;
                }

                /* Assert the port doesn't belong to any LAG */
                assert(!port.m_lag_id && !port.m_lag_member_id);

                if (addLagMember(lag, port))
                    it = consumer.m_toSync.erase(it);
                else
                    it++;
            }
            /* Sync an disabled member */
            else /* status == "disabled" */
            {
                /* "status" is "disabled" at start when m_lag_id and
                 * m_lag_member_id are absent */
                if (!port.m_lag_id || !port.m_lag_member_id)
                {
                    it = consumer.m_toSync.erase(it);
                    continue;
                }

                if (removeLagMember(lag, port))
                    it = consumer.m_toSync.erase(it);
                else
                    it++;
            }
        }
        /* Remove a LAG member */
        else if (op == DEL_COMMAND)
        {
            /* Assert the LAG member exists */
            assert(lag.m_members.find(port_alias) != lag.m_members.end());

            /* Assert the port belongs to a LAG */
            assert(port.m_lag_id && port.m_lag_member_id);

            if (removeLagMember(lag, port))
                it = consumer.m_toSync.erase(it);
            else
                it++;
        }
        else
        {
            SWSS_LOG_ERROR("Unknown operation type %s", op.c_str());
            it = consumer.m_toSync.erase(it);
        }
    }
}

void PortsOrch::doTask(Consumer &consumer)
{
    SWSS_LOG_ENTER();

    string table_name = consumer.m_consumer->getTableName();

    if (table_name == APP_PORT_TABLE_NAME)
        doPortTask(consumer);
    else if (table_name == APP_VLAN_TABLE_NAME)
        doVlanTask(consumer);
    else if (table_name == APP_VLAN_MEMBER_TABLE_NAME)
        doVlanMemberTask(consumer);
    else if (table_name == APP_LAG_TABLE_NAME)
        doLagTask(consumer);
    else if (table_name == APP_LAG_MEMBER_TABLE_NAME)
        doLagMemberTask(consumer);
}

void PortsOrch::initializeQueues(Port &port)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES;
    sai_status_t status = sai_port_api->get_port_attribute(port.m_port_id, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get number of queues for port %s rv:%d", port.m_alias.c_str(), status);
        throw runtime_error("PortsOrch initialization failure.");
    }
    SWSS_LOG_INFO("Get %d queues for port %s", attr.value.u32, port.m_alias.c_str());

    port.m_queue_ids.resize(attr.value.u32);

    if (attr.value.u32 == 0)
    {
        return;
    }

    attr.id = SAI_PORT_ATTR_QOS_QUEUE_LIST;
    attr.value.objlist.count = port.m_queue_ids.size();
    attr.value.objlist.list = port.m_queue_ids.data();

    status = sai_port_api->get_port_attribute(port.m_port_id, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get queue list for port %s rv:%d", port.m_alias.c_str(), status);
        throw runtime_error("PortsOrch initialization failure.");
    }
    SWSS_LOG_INFO("Get queues for port %s", port.m_alias.c_str());
}

void PortsOrch::initializePriorityGroups(Port &port)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_NUMBER_OF_INGRESS_PRIORITY_GROUPS;
    sai_status_t status = sai_port_api->get_port_attribute(port.m_port_id, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get number of priority groups for port %s rv:%d", port.m_alias.c_str(), status);
        throw runtime_error("PortsOrch initialization failure.");
    }
    SWSS_LOG_INFO("Get %d priority groups for port %s", attr.value.u32, port.m_alias.c_str());

    port.m_priority_group_ids.resize(attr.value.u32);

    if (attr.value.u32 == 0)
    {
        return;
    }

    attr.id = SAI_PORT_ATTR_INGRESS_PRIORITY_GROUP_LIST;
    attr.value.objlist.count = port.m_priority_group_ids.size();
    attr.value.objlist.list = port.m_priority_group_ids.data();

    status = sai_port_api->get_port_attribute(port.m_port_id, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Fail to get priority group list for port %s rv:%d", port.m_alias.c_str(), status);
        throw runtime_error("PortsOrch initialization failure.");
    }
    SWSS_LOG_INFO("Get priority groups for port %s", port.m_alias.c_str());
}

bool PortsOrch::initializePort(Port &p)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("Initializing port alias:%s pid:%lx", p.m_alias.c_str(), p.m_port_id);

    initializePriorityGroups(p);
    initializeQueues(p);

    /* Create host interface */
    addHostIntfs(p.m_port_id, p.m_alias, p.m_hif_id);

#if 0
    // TODO: Assure if_nametoindex(p.m_alias.c_str()) != 0
    p.m_ifindex = if_nametoindex(p.m_alias.c_str());
    if (p.m_ifindex == 0)
    {
        SWSS_LOG_ERROR("Failed to get netdev index alias:%s", p.m_alias.c_str());
        return false;
    }
#endif

    /* Set default port admin status to DOWN */
    setPortAdminStatus(p.m_port_id, false);

    /**
     * Create default database port oper status as DOWN
     * This status will be updated when receiving port_oper_status_notification.
     */
    vector<FieldValueTuple> vector;
    FieldValueTuple tuple("oper_status", "down");
    vector.push_back(tuple);
    m_portTable->set(p.m_alias, vector);

    return true;
}

bool PortsOrch::addHostIntfs(sai_object_id_t id, string alias, sai_object_id_t &host_intfs_id)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_HOSTIF_ATTR_TYPE;
    attr.value.s32 = SAI_HOSTIF_TYPE_NETDEV;
    attrs.push_back(attr);

    attr.id = SAI_HOSTIF_ATTR_OBJ_ID;
    attr.value.oid = id;
    attrs.push_back(attr);

    attr.id = SAI_HOSTIF_ATTR_NAME;
    strncpy((char *)&attr.value.chardata, alias.c_str(), HOSTIF_NAME_SIZE);
    attrs.push_back(attr);

    sai_status_t status = sai_hostif_api->create_hostif(&host_intfs_id, gSwitchId, attrs.size(), attrs.data());
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to create host interface for port %s", alias.c_str());
        return false;
    }

    SWSS_LOG_NOTICE("Create host interface for port %s", alias.c_str());

    return true;
}

bool PortsOrch::addVlan(string vlan_alias)
{
    SWSS_LOG_ENTER();

    // TODO: how to use vlan_oid??
    sai_object_id_t vlan_oid;

    sai_vlan_id_t vlan_id = stoi(vlan_alias.substr(4));
    sai_attribute_t attr;
    attr.id = SAI_VLAN_ATTR_VLAN_ID;
    attr.value.u16 = vlan_id;
    sai_status_t status = sai_vlan_api->create_vlan(&vlan_oid, gSwitchId, 1, &attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to create VLAN %s vid:%hu", vlan_alias.c_str(), vlan_id);
        return false;
    }

    SWSS_LOG_NOTICE("Create an empty VLAN %s vid:%hu", vlan_alias.c_str(), vlan_id);

/*
    // create vlan rif(s)
    sai_attribute_t rif_attr_list[2];
    rif_attr_list[0].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
    rif_attr_list[0].value = vlan_id;
    rif_attr_list[1].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
    rif_attr_list[1].value = mac;
    sai_create_rif_fn(&rif1, 2, rif_list);
*/

    Port vlan(vlan_alias, Port::VLAN);
    vlan.m_vlan_id = vlan_id;
    vlan.m_members = set<string>();
    m_portList[vlan_alias] = vlan;

    return true;
}

bool PortsOrch::removeVlan(Port vlan)
{
    SWSS_LOG_ENTER();

    /* Retry when the VLAN still has members */
    if (vlan.m_members.size() > 0)
    {
        SWSS_LOG_ERROR("Failed to remove non-empty VLAN %s", vlan.m_alias.c_str());
        return false;
    }

    sai_status_t status = sai_vlan_api->remove_vlan(vlan.m_vlan_id);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to remove VLAN %s vid:%hu", vlan.m_alias.c_str(), vlan.m_vlan_id);
        return false;
    }

    SWSS_LOG_NOTICE("Remove VLAN %s vid:%hu", vlan.m_alias.c_str(), vlan.m_vlan_id);

    m_portList.erase(vlan.m_alias);

    return true;
}

bool PortsOrch::addVlanMember(Port vlan, Port port, string& tagging_mode)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
    attr.value.u16 = vlan.m_vlan_id;
    attrs.push_back(attr);

    attr.id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    attr.value.oid = m_bridgePort.left.at(port.m_port_id);
    attrs.push_back(attr);

    attr.id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;              
    if (tagging_mode == "untagged")
        attr.value.s32 = SAI_VLAN_TAGGING_MODE_UNTAGGED;
    else if (tagging_mode == "tagged")
        attr.value.s32 = SAI_VLAN_TAGGING_MODE_TAGGED;
    else if (tagging_mode == "priority_tagged")
        attr.value.s32 = SAI_VLAN_TAGGING_MODE_PRIORITY_TAGGED;
    else assert(false);
    attrs.push_back(attr);

    sai_object_id_t vlan_member_id;
    sai_status_t status = sai_vlan_api->create_vlan_member(&vlan_member_id, gSwitchId, attrs.size(), attrs.data());
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to add member %s to VLAN %s vid:%hu pid:%lx",
                port.m_alias.c_str(), vlan.m_alias.c_str(), vlan.m_vlan_id, port.m_port_id);
        return false;
    }
    SWSS_LOG_NOTICE("Add member %s to VLAN %s vid:%hu pid%lx",
            port.m_alias.c_str(), vlan.m_alias.c_str(), vlan.m_vlan_id, port.m_port_id);

    // Set port bind mode
    sai_attribute_t port_attr;
    port_attr.id = SAI_PORT_ATTR_BIND_MODE;
    port_attr.value.s32 = SAI_PORT_BIND_MODE_PORT;
    status = sai_port_api->set_port_attribute(port.m_port_id, &port_attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to set port pid:%lx bind mode to port: %d",
                port.m_port_id, status);
        return false;
    }

    port.m_vlan_id = vlan.m_vlan_id;
    port.m_port_vlan_id = vlan.m_vlan_id;
    port.m_vlan_member_id = vlan_member_id;
    m_portList[port.m_alias] = port;
    vlan.m_members.insert(port.m_alias);
    m_portList[vlan.m_alias] = vlan;

    VlanMemberUpdate update = { vlan, port, true };
    notify(SUBJECT_TYPE_VLAN_MEMBER_CHANGE, static_cast<void *>(&update));

    return true;
}

bool PortsOrch::removeVlanMember(Port vlan, Port port)
{
    SWSS_LOG_ENTER();

    sai_status_t status = sai_vlan_api->remove_vlan_member(port.m_vlan_member_id);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to remove member %s from VLAN %s vid:%hx vmid:%lx",
                port.m_alias.c_str(), vlan.m_alias.c_str(), vlan.m_vlan_id, port.m_vlan_member_id);
        return false;
    }

    SWSS_LOG_NOTICE("Remove member %s from VLAN %s lid:%hx vmid:%lx",
            port.m_alias.c_str(), vlan.m_alias.c_str(), vlan.m_vlan_id, port.m_vlan_member_id);

    sai_attribute_t attr;
    attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;
    attr.value.u16 = DEFAULT_PORT_VLAN_ID;

    status = sai_port_api->set_port_attribute(port.m_port_id, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to reset port VLAN ID to DEFAULT_PORT_VLAN_ID pid:%lx",
                port.m_port_id);
        return false;
    }

    port.m_vlan_id = 0;
    port.m_port_vlan_id = DEFAULT_PORT_VLAN_ID;
    port.m_vlan_member_id = 0;
    m_portList[port.m_alias] = port;
    vlan.m_members.erase(port.m_alias);
    m_portList[vlan.m_alias] = vlan;

    VlanMemberUpdate update = { vlan, port, false };
    notify(SUBJECT_TYPE_VLAN_MEMBER_CHANGE, static_cast<void *>(&update));

    return true;
}

bool PortsOrch::addLag(string lag_alias)
{
    SWSS_LOG_ENTER();

    sai_object_id_t lag_id;
    sai_status_t status = sai_lag_api->create_lag(&lag_id, gSwitchId, 0, NULL);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to create LAG %s lid:%lx", lag_alias.c_str(), lag_id);
        return false;
    }

    SWSS_LOG_NOTICE("Create an empty LAG %s lid:%lx", lag_alias.c_str(), lag_id);

    Port lag(lag_alias, Port::LAG);
    lag.m_lag_id = lag_id;
    lag.m_members = set<string>();
    m_portList[lag_alias] = lag;

    return true;
}

bool PortsOrch::removeLag(Port lag)
{
    SWSS_LOG_ENTER();

    /* Retry when the LAG still has members */
    if (lag.m_members.size() > 0)
    {
        SWSS_LOG_ERROR("Failed to remove non-empty LAG %s", lag.m_alias.c_str());
        return false;
    }

    sai_status_t status = sai_lag_api->remove_lag(lag.m_lag_id);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to remove LAG %s lid:%lx", lag.m_alias.c_str(), lag.m_lag_id);
        return false;
    }

    SWSS_LOG_NOTICE("Remove LAG %s lid:%lx", lag.m_alias.c_str(), lag.m_lag_id);

    m_portList.erase(lag.m_alias);

    return true;
}

bool PortsOrch::addLagMember(Port lag, Port port)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_LAG_MEMBER_ATTR_LAG_ID;
    attr.value.oid = lag.m_lag_id;
    attrs.push_back(attr);

    attr.id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    attr.value.oid = port.m_port_id;
    attrs.push_back(attr);

    sai_object_id_t lag_member_id;
    sai_status_t status = sai_lag_api->create_lag_member(&lag_member_id, gSwitchId, attrs.size(), attrs.data());

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to add member %s to LAG %s lid:%lx pid:%lx",
                port.m_alias.c_str(), lag.m_alias.c_str(), lag.m_lag_id, port.m_port_id);
        return false;
    }

    SWSS_LOG_NOTICE("Add member %s to LAG %s lid:%lx pid:%lx",
            port.m_alias.c_str(), lag.m_alias.c_str(), lag.m_lag_id, port.m_port_id);

    port.m_lag_id = lag.m_lag_id;
    port.m_lag_member_id = lag_member_id;
    m_portList[port.m_alias] = port;
    lag.m_members.insert(port.m_alias);

    m_portList[lag.m_alias] = lag;

    LagMemberUpdate update = { lag, port, true };
    notify(SUBJECT_TYPE_LAG_MEMBER_CHANGE, static_cast<void *>(&update));

    return true;
}

bool PortsOrch::removeLagMember(Port lag, Port port)
{
    sai_status_t status = sai_lag_api->remove_lag_member(port.m_lag_member_id);

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to remove member %s from LAG %s lid:%lx lmid:%lx",
                port.m_alias.c_str(), lag.m_alias.c_str(), lag.m_lag_id, port.m_lag_member_id);
        return false;
    }

    SWSS_LOG_NOTICE("Remove member %s from LAG %s lid:%lx lmid:%lx",
            port.m_alias.c_str(), lag.m_alias.c_str(), lag.m_lag_id, port.m_lag_member_id);

    port.m_lag_id = 0;
    port.m_lag_member_id = 0;
    m_portList[port.m_alias] = port;
    lag.m_members.erase(port.m_alias);
    m_portList[lag.m_alias] = lag;

    LagMemberUpdate update = { lag, port, false };
    notify(SUBJECT_TYPE_LAG_MEMBER_CHANGE, static_cast<void *>(&update));

    return true;
}

bool PortsOrch::removeBridgePort(Port port)
{
    // port id -> bridge port id
    sai_object_id_t port_id = port.m_port_id;
    auto foundBridgePort = m_bridgePort.left.find(port_id);
    if (foundBridgePort == m_bridgePort.left.end())
    {
        SWSS_LOG_ERROR("Failed to find port %lx in default 1Q_BRIDGE", port_id);
        return false;
    }
    sai_object_id_t bridge_port_id = foundBridgePort->second;

    // Remove vlan membership of this bridge port
    auto foundVlanMember = m_bridgePortVlanMember.find(bridge_port_id);
    if (foundVlanMember == m_bridgePortVlanMember.end())
    {
        SWSS_LOG_ERROR("Failed to find bridge port %lx in default VLAN", bridge_port_id);
        return false;
    }
    sai_object_id_t vlan_member_id = foundVlanMember->second;
    m_bridgePortVlanMember.erase(foundVlanMember);
    sai_status_t status = sai_vlan_api->remove_vlan_member(vlan_member_id);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to remove vlan member %lx from default VLAN: %d", vlan_member_id, status);
        return false;
    }

    // Remove bridge port from default 1Q bridge
    m_bridgePort.left.erase(foundBridgePort);
    status = sai_bridge_api->remove_bridge_port(bridge_port_id);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to remove port %lx from default 1Q_BRIDGE: %d", bridge_port_id, status);
        return false;
    }

    return true;
}

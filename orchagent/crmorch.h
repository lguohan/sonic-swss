#pragma once

#include <thread>
#include <chrono>
#include <map>
#include "orch.h"
#include "port.h"

extern "C" {
#include "sai.h"
}

using namespace std;

enum class CrmResourceType
{
    CRM_IPV4_ROUTE,
    CRM_IPV6_ROUTE,
    CRM_IPV4_NEXTHOP,
    CRM_IPV6_NEXTHOP,
    CRM_IPV4_NEIGHBOR,
    CRM_IPV6_NEIGHBOR,
    CRM_NEXTHOP_GROUP_MEMBER,
    CRM_NEXTHOP_GROUP_OBJECT,
    CRM_ACL_TABLE,
    CRM_ACL_GROUP,
    CRM_ACL_ENTRY,
    CRM_ACL_COUNTER,
    CRM_FDB_ENTRY,
};

enum class CrmThresholdType
{
    PERCENTAGE,
    USED,
    FREE,
};

class CrmOrch : public Orch
{
public:
    CrmOrch(DBConnector *db, string tableName);
    void incCrmResUsedCounter(CrmResourceType resource);
    void decCrmResUsedCounter(CrmResourceType resource);
    void incCrmAclUsedCounter(CrmResourceType resource, sai_acl_stage_t stage, sai_acl_bind_point_type_t point);
    void decCrmAclUsedCounter(CrmResourceType resource, sai_acl_stage_t stage, sai_acl_bind_point_type_t point);
    void incCrmAclTableUsedCounter(CrmResourceType resource, sai_object_id_t tableId);
    void decCrmAclTableUsedCounter(CrmResourceType resource, sai_object_id_t tableId);

private:
    shared_ptr<DBConnector> m_countersDb = nullptr;
    shared_ptr<Table> m_countersCrmTable = nullptr;

    struct CrmResourceCounter
    {
        sai_object_id_t id = 0;
        uint32_t availableCounter = 0;
        uint32_t usedCounter = 0;
    };

    struct CrmResourceEntry
    {
        CrmResourceEntry(string name, CrmThresholdType thresholdType, uint32_t lowThreshold, uint32_t highThreshold);

        string name;

        CrmThresholdType thresholdType = CrmThresholdType::PERCENTAGE;
        uint32_t lowThreshold = 0;
        uint32_t highThreshold = 0;

        map<string, CrmResourceCounter> countersMap;

        uint32_t clearLogCounter = 0;
        uint32_t exceededLogCounter = 0;
    };

    chrono::minutes m_pollingInterval;

    map<CrmResourceType, CrmResourceEntry> m_resourcesMap;

    shared_ptr<thread> m_crmThread = nullptr;

    void doTask(Consumer &consumer);
    void handleSetCommand(const string& key, const vector<FieldValueTuple>& data);
    void crmThread();
    void startCrmThread();
    void getResAvailableCounters();
    void updateCrmCountersTable();
    void checkCrmThresholds();
    string getCrmAclKey(sai_acl_stage_t stage, sai_acl_bind_point_type_t bindPoint);
    string getCrmAclTableKey(sai_object_id_t id);
};

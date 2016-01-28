/*
 * =======================================================================================
 *
 *      Filename:  luawid.c
 *
 *      Description:  C part of the Likwid Lua interface
 *
 *      Version:   <VERSION>
 *      Released:  <DATE>
 *
 *      Author:   Thomas Roehl (tr), thomas.roehl@googlemail.com
 *      Project:  likwid
 *
 *      Copyright (C) 2015 RRZE, University Erlangen-Nuremberg
 *
 *      This program is free software: you can redistribute it and/or modify it under
 *      the terms of the GNU General Public License as published by the Free Software
 *      Foundation, either version 3 of the License, or (at your option) any later
 *      version.
 *
 *      This program is distributed in the hope that it will be useful, but WITHOUT ANY
 *      WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 *      PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along with
 *      this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * =======================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sched.h>

#include <lua.h>                               /* Always include this */
#include <lauxlib.h>                           /* Always include this */
#include <lualib.h>                            /* Always include this */

#include <likwid.h>
#include <tree.h>
#include <access.h>

#ifdef COLOR
#include <textcolor.h>
#endif

static int topology_isInitialized = 0;
CpuInfo_t cpuinfo = NULL;
CpuTopology_t cputopo = NULL;

static int numa_isInitialized = 0;
NumaTopology_t numainfo = NULL;
static int affinity_isInitialized = 0;
AffinityDomains_t affinity = NULL;
static int perfmon_isInitialized = 0;
static int timer_isInitialized = 0;
static int power_isInitialized = 0;
PowerInfo_t power;
static int power_hasRAPL = 0;
static int config_isInitialized = 0;
Configuration_t configfile = NULL;


static int lua_likwid_getConfiguration(lua_State* L)
{
    int ret = 0;
    if (config_isInitialized == 0)
    {
        ret = init_configuration();
        if (ret == 0)
        {
            config_isInitialized = 1;
            configfile = get_configuration();
        }
        else
        {
            lua_newtable(L);
            lua_pushstring(L, "configFile");
            lua_pushnil(L);
            lua_settable(L,-3);
            lua_pushstring(L, "topologyFile");
            lua_pushnil(L);
            lua_settable(L,-3);
            lua_pushstring(L, "daemonPath");
            lua_pushnil(L);
            lua_settable(L,-3);
            lua_pushstring(L, "daemonMode");
            lua_pushinteger(L, -1);
            lua_settable(L,-3);
            lua_pushstring(L, "maxNumThreads");
            lua_pushinteger(L, MAX_NUM_THREADS);
            lua_settable(L,-3);
            lua_pushstring(L, "maxNumNodes");
            lua_pushinteger(L, MAX_NUM_NODES);
            lua_settable(L,-3);
            return 1;
        }
    }
    if ((config_isInitialized) && (configfile == NULL))
    {
        configfile = get_configuration();
    }
    lua_newtable(L);
    lua_pushstring(L, "configFile");
    lua_pushstring(L, configfile->configFileName);
    lua_settable(L,-3);
    lua_pushstring(L, "topologyFile");
    lua_pushstring(L, configfile->topologyCfgFileName);
    lua_settable(L,-3);
    lua_pushstring(L, "daemonPath");
    lua_pushstring(L, configfile->daemonPath);
    lua_settable(L,-3);
    lua_pushstring(L, "daemonMode");
    lua_pushinteger(L, (int)configfile->daemonMode);
    lua_settable(L,-3);
    lua_pushstring(L, "maxNumThreads");
    lua_pushinteger(L, configfile->maxNumThreads);
    lua_settable(L,-3);
    lua_pushstring(L, "maxNumNodes");
    lua_pushinteger(L, configfile->maxNumNodes);
    lua_settable(L,-3);
    return 1;
}

static int lua_likwid_putConfiguration(lua_State* L)
{
    if (config_isInitialized == 1)
    {
        destroy_configuration();
        config_isInitialized = 0;
        configfile = NULL;
    }
    return 0;
}

static int lua_likwid_setAccessMode(lua_State* L)
{
    int flag;
    flag = luaL_checknumber(L,1);
    luaL_argcheck(L, flag >= 0 && flag <= 1, 1, "invalid access mode, only 0 (direct) and 1 (accessdaemon) allowed");
    HPMmode(flag);
    lua_pushnumber(L,0);
    return 1;
}

static int lua_likwid_init(lua_State* L)
{
    int ret;
    int nrThreads = luaL_checknumber(L,1);
    luaL_argcheck(L, nrThreads > 0, 1, "CPU count must be greater than 0");
    int cpus[nrThreads];
    if (!lua_istable(L, -1)) {
      lua_pushstring(L,"No table given as second argument");
      lua_error(L);
    }
    for (ret = 1; ret<=nrThreads; ret++)
    {
        lua_rawgeti(L,-1,ret);
        cpus[ret-1] = lua_tounsigned(L,-1);
        lua_pop(L,1);
    }
    if (topology_isInitialized == 0)
    {
        topology_init();
        topology_isInitialized = 1;
        cpuinfo = get_cpuInfo();
        cputopo = get_cpuTopology();
    }
    if ((topology_isInitialized) && (cpuinfo == NULL))
    {
        cpuinfo = get_cpuInfo();
    }
    if ((topology_isInitialized) && (cputopo == NULL))
    {
        cputopo = get_cpuTopology();
    }
    if (numa_isInitialized == 0)
    {
        numa_init();
        numa_isInitialized = 1;
        numainfo = get_numaTopology();
    }
    if ((numa_isInitialized) && (numainfo == NULL))
    {
        numainfo = get_numaTopology();
    }
    if (perfmon_isInitialized == 0)
    {
        ret = perfmon_init(nrThreads, &(cpus[0]));
        if (ret != 0)
        {
            lua_pushstring(L,"Cannot initialize likwid perfmon");
            lua_error(L);
            return 1;
        }
        perfmon_isInitialized = 1;
        timer_isInitialized = 1;
        lua_pushinteger(L,ret);
    }
    return 1;
}


static int lua_likwid_addEventSet(lua_State* L)
{
    int groupId, n;
    const char* tmpString;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    n = lua_gettop(L);
    tmpString = luaL_checkstring(L, n);
    luaL_argcheck(L, strlen(tmpString) > 0, n, "Event string must be larger than 0");

    groupId = perfmon_addEventSet((char*)tmpString);
    lua_pushnumber(L, groupId+1);
    return 1;
}

static int lua_likwid_setupCounters(lua_State* L)
{
    int ret;
    int groupId = lua_tonumber(L,1);
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    ret = perfmon_setupCounters(groupId-1);
    lua_pushnumber(L,ret);
    return 1;
}


static int lua_likwid_startCounters(lua_State* L)
{
    int ret;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    ret = perfmon_startCounters();
    lua_pushnumber(L,ret);
    return 1;
}

static int lua_likwid_stopCounters(lua_State* L)
{
    int ret;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    ret = perfmon_stopCounters();
    lua_pushnumber(L,ret);
    return 1;
}

static int lua_likwid_readCounters(lua_State* L)
{
    int ret;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    ret = perfmon_readCounters();
    lua_pushnumber(L,ret);
    return 1;
}

static int lua_likwid_switchGroup(lua_State* L)
{
    int ret = -1;
    int newgroup = lua_tonumber(L,1)-1;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    if (newgroup >= perfmon_getNumberOfGroups())
    {
        newgroup = 0;
    }
    if (newgroup == perfmon_getIdOfActiveGroup())
    {
        lua_pushinteger(L, ret);
        return 1;
    }
    ret = perfmon_switchActiveGroup(newgroup);
    lua_pushinteger(L, ret);
    return 1;
}

static int lua_likwid_finalize(lua_State* L)
{
    if (perfmon_isInitialized == 1)
    {
        perfmon_finalize();
        perfmon_isInitialized = 0;
    }
    if (affinity_isInitialized == 1)
    {
        affinity_finalize();
        affinity_isInitialized = 0;
        affinity = NULL;
    }
    if (numa_isInitialized == 1)
    {
        numa_finalize();
        numa_isInitialized = 0;
        numainfo = NULL;
    }
    if (topology_isInitialized == 1)
    {
        topology_finalize();
        topology_isInitialized = 0;
        cputopo = NULL;
        cpuinfo = NULL;
    }
    if (config_isInitialized == 1)
    {
        destroy_configuration();
        config_isInitialized = 0;
        configfile = NULL;
    }
    return 0;
}

static int lua_likwid_getResult(lua_State* L)
{
    int groupId, eventId, threadId;
    double result = 0;
    groupId = lua_tonumber(L,1);
    eventId = lua_tonumber(L,2);
    threadId = lua_tonumber(L,3);
    result = perfmon_getResult(groupId-1, eventId-1, threadId-1);
    lua_pushnumber(L,result);
    return 1;
}

static int lua_likwid_getNumberOfGroups(lua_State* L)
{
    int number;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    number = perfmon_getNumberOfGroups();
    lua_pushnumber(L,number);
    return 1;
}

static int lua_likwid_getIdOfActiveGroup(lua_State* L)
{
    int number;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    number = perfmon_getIdOfActiveGroup();
    lua_pushnumber(L,number+1);
    return 1;
}

static int lua_likwid_getRuntimeOfGroup(lua_State* L)
{
    double time;
    int groupId;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    groupId = lua_tonumber(L,1);
    time = perfmon_getTimeOfGroup(groupId-1);
    lua_pushnumber(L, time);
    return 1;
}

static int lua_likwid_getNumberOfEvents(lua_State* L)
{
    int number, groupId;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    groupId = lua_tonumber(L,1);
    number = perfmon_getNumberOfEvents(groupId-1);
    lua_pushnumber(L,number);
    return 1;
}

static int lua_likwid_getNumberOfThreads(lua_State* L)
{
    int number;
    if (perfmon_isInitialized == 0)
    {
        return 0;
    }
    number = perfmon_getNumberOfThreads();
    lua_pushnumber(L,number);
    return 1;
}

static int lua_likwid_printSupportedCPUs(lua_State* L)
{
    print_supportedCPUs();
    return 0;
}

static int lua_likwid_getCpuInfo(lua_State* L)
{
    if (topology_isInitialized == 0)
    {
        topology_init();
        topology_isInitialized = 1;
        cpuinfo = get_cpuInfo();
    }
    if ((topology_isInitialized) && (cpuinfo == NULL))
    {
        cpuinfo = get_cpuInfo();
    }
    lua_newtable(L);
    lua_pushstring(L,"family");
    lua_pushunsigned(L,cpuinfo->family);
    lua_settable(L,-3);
    lua_pushstring(L,"model");
    lua_pushunsigned(L,cpuinfo->model);
    lua_settable(L,-3);
    lua_pushstring(L,"stepping");
    lua_pushunsigned(L,cpuinfo->stepping);
    lua_settable(L,-3);
    lua_pushstring(L,"clock");
    lua_pushunsigned(L,cpuinfo->clock);
    lua_settable(L,-3);
    lua_pushstring(L,"turbo");
    lua_pushinteger(L,cpuinfo->turbo);
    lua_settable(L,-3);
    lua_pushstring(L,"name");
    lua_pushstring(L,cpuinfo->name);
    lua_settable(L,-3);
    lua_pushstring(L,"osname");
    lua_pushstring(L,cpuinfo->osname);
    lua_settable(L,-3);
    lua_pushstring(L,"short_name");
    lua_pushstring(L,cpuinfo->short_name);
    lua_settable(L,-3);
    lua_pushstring(L,"features");
    lua_pushstring(L,cpuinfo->features);
    lua_settable(L,-3);
    lua_pushstring(L,"isIntel");
    lua_pushinteger(L,cpuinfo->isIntel);
    lua_settable(L,-3);
    lua_pushstring(L,"featureFlags");
    lua_pushunsigned(L,cpuinfo->featureFlags);
    lua_settable(L,-3);
    lua_pushstring(L,"perf_version");
    lua_pushunsigned(L, cpuinfo->perf_version);
    lua_settable(L,-3);
    lua_pushstring(L,"perf_num_ctr");
    lua_pushunsigned(L,cpuinfo->perf_num_ctr);
    lua_settable(L,-3);
    lua_pushstring(L,"perf_width_ctr");
    lua_pushunsigned(L,cpuinfo->perf_width_ctr);
    lua_settable(L,-3);
    lua_pushstring(L,"perf_num_fixed_ctr");
    lua_pushunsigned(L,cpuinfo->perf_num_fixed_ctr);
    lua_settable(L,-3);
    return 1;
}

static int lua_likwid_getCpuTopology(lua_State* L)
{
    int i;
    TreeNode* socketNode;
    int socketCount = 0;
    TreeNode* coreNode;
    int coreCount = 0;
    TreeNode* threadNode;
    int threadCount = 0;
    if (topology_isInitialized == 0)
    {
        topology_init();
        topology_isInitialized = 1;
        cputopo = get_cpuTopology();
    }
    if ((topology_isInitialized) && (cputopo == NULL))
    {
        cputopo = get_cpuTopology();
    }
    if (numa_isInitialized == 0)
    {
        if (numa_init() == 0)
        {
            numa_isInitialized = 1;
            numainfo = get_numaTopology();
        }
    }
    if ((numa_isInitialized) && (numainfo == NULL))
    {
        numainfo = get_numaTopology();
    }

    lua_newtable(L);

    lua_pushstring(L,"numHWThreads");
    lua_pushunsigned(L,cputopo->numHWThreads);
    lua_settable(L,-3);

    lua_pushstring(L,"activeHWThreads");
    lua_pushunsigned(L,cputopo->activeHWThreads);
    lua_settable(L,-3);

    lua_pushstring(L,"numSockets");
    lua_pushunsigned(L,cputopo->numSockets);
    lua_settable(L,-3);

    lua_pushstring(L,"numCoresPerSocket");
    lua_pushunsigned(L,cputopo->numCoresPerSocket);
    lua_settable(L,-3);

    lua_pushstring(L,"numThreadsPerCore");
    lua_pushunsigned(L,cputopo->numThreadsPerCore);
    lua_settable(L,-3);

    lua_pushstring(L,"numCacheLevels");
    lua_pushinteger(L,cputopo->numCacheLevels);
    lua_settable(L,-3);

    lua_pushstring(L,"threadPool");
    lua_newtable(L);
    for(i=0;i<cputopo->numHWThreads;i++)
    {
        lua_pushnumber(L,i);
        lua_newtable(L);
        lua_pushstring(L,"threadId");
        lua_pushunsigned(L,cputopo->threadPool[i].threadId);
        lua_settable(L,-3);
        lua_pushstring(L,"coreId");
        lua_pushunsigned(L,cputopo->threadPool[i].coreId);
        lua_settable(L,-3);
        lua_pushstring(L,"packageId");
        lua_pushunsigned(L,cputopo->threadPool[i].packageId);
        lua_settable(L,-3);
        lua_pushstring(L,"apicId");
        lua_pushunsigned(L,cputopo->threadPool[i].apicId);
        lua_settable(L,-3);
        lua_pushstring(L,"inCpuSet");
        lua_pushunsigned(L,cputopo->threadPool[i].inCpuSet);
        lua_settable(L,-3);
        lua_settable(L,-3);
    }
    lua_settable(L,-3);

    lua_pushstring(L,"cacheLevels");
    lua_newtable(L);
    for(i=0;i<cputopo->numCacheLevels;i++)
    {
        lua_pushnumber(L,i+1);
        lua_newtable(L);

        lua_pushstring(L,"level");
        lua_pushunsigned(L,cputopo->cacheLevels[i].level);
        lua_settable(L,-3);

        lua_pushstring(L,"associativity");
        lua_pushunsigned(L,cputopo->cacheLevels[i].associativity);
        lua_settable(L,-3);

        lua_pushstring(L,"sets");
        lua_pushunsigned(L,cputopo->cacheLevels[i].sets);
        lua_settable(L,-3);

        lua_pushstring(L,"lineSize");
        lua_pushunsigned(L,cputopo->cacheLevels[i].lineSize);
        lua_settable(L,-3);

        lua_pushstring(L,"size");
        lua_pushunsigned(L,cputopo->cacheLevels[i].size);
        lua_settable(L,-3);

        lua_pushstring(L,"threads");
        lua_pushunsigned(L,cputopo->cacheLevels[i].threads);
        lua_settable(L,-3);

        lua_pushstring(L,"inclusive");
        lua_pushunsigned(L,cputopo->cacheLevels[i].inclusive);
        lua_settable(L,-3);

        lua_pushstring(L,"type");
        switch (cputopo->cacheLevels[i].type)
        {
            case DATACACHE:
                lua_pushstring(L,"DATACACHE");
                break;
            case INSTRUCTIONCACHE:
                lua_pushstring(L,"INSTRUCTIONCACHE");
                break;
            case UNIFIEDCACHE:
                lua_pushstring(L,"UNIFIEDCACHE");
                break;
            case ITLB:
                lua_pushstring(L,"ITLB");
                break;
            case DTLB:
                lua_pushstring(L,"DTLB");
                break;
            case NOCACHE:
            default:
                lua_pushstring(L,"NOCACHE");
                break;
        }
        lua_settable(L,-3);
        lua_settable(L,-3);
    }
    lua_settable(L,-3);

    lua_pushstring(L,"topologyTree");
    lua_newtable(L);

    socketNode = tree_getChildNode(cputopo->topologyTree);
    while (socketNode != NULL)
    {
        lua_pushinteger(L, socketCount);
        lua_newtable(L);
        lua_pushstring(L, "ID");
        lua_pushunsigned(L,socketNode->id);
        lua_settable(L, -3);
        lua_pushstring(L, "Childs");
        lua_newtable(L);
        coreCount = 0;
        coreNode = tree_getChildNode(socketNode);
        while (coreNode != NULL)
        {
            lua_pushinteger(L, coreCount);
            lua_newtable(L);
            lua_pushstring(L, "ID");
            lua_pushunsigned(L,coreNode->id);
            lua_settable(L,-3);
            lua_pushstring(L, "Childs");
            lua_newtable(L);
            threadNode = tree_getChildNode(coreNode);
            threadCount = 0;
            while (threadNode != NULL)
            {
                lua_pushunsigned(L,threadCount);
                lua_pushunsigned(L,threadNode->id);
                lua_settable(L,-3);
                threadNode = tree_getNextNode(threadNode);
                threadCount++;
            }
            lua_settable(L,-3);
            coreNode = tree_getNextNode(coreNode);
            coreCount++;
            lua_settable(L,-3);
        }
        lua_settable(L,-3);
        socketNode = tree_getNextNode(socketNode);
        socketCount++;
        lua_settable(L,-3);
    }
    lua_settable(L,-3);
    return 1;
}

static int lua_likwid_putTopology(lua_State* L)
{
    if (topology_isInitialized == 1)
    {
        topology_finalize();
        topology_isInitialized = 0;
        cpuinfo = NULL;
        cputopo = NULL;
    }
    return 0;
}


static int lua_likwid_getEventsAndCounters(lua_State* L)
{
    int i;
    char optString[1024];
    int optStringIndex = 0;
    if (topology_isInitialized == 0)
    {
        topology_init();
        topology_isInitialized = 1;
        cpuinfo = get_cpuInfo();
    }
    if ((topology_isInitialized) && (cpuinfo == NULL))
    {
        cpuinfo = get_cpuInfo();
    }
    perfmon_init_maps();
    lua_newtable(L);
    lua_pushstring(L,"Counters");
    lua_newtable(L);
    for(i=1;i<=perfmon_numCounters;i++)
    {
        optStringIndex = 0;
        optString[0] = '\0';
        lua_pushunsigned(L,i);
        lua_newtable(L);
        lua_pushstring(L,"Name");
        lua_pushstring(L,counter_map[i-1].key);
        lua_settable(L,-3);
        lua_pushstring(L,"Options");
        for(int j=1; j<NUM_EVENT_OPTIONS; j++)
        {
            if (counter_map[i-1].optionMask & REG_TYPE_MASK(j))
            {
                optStringIndex += sprintf(&(optString[optStringIndex]), "%s|", eventOptionTypeName[j]);
            }
        }
        optString[optStringIndex-1] = '\0';
        lua_pushstring(L,optString);
        lua_settable(L,-3);
        lua_pushstring(L,"Type");
        lua_pushunsigned(L, counter_map[i-1].type);
        lua_settable(L,-3);
        lua_pushstring(L,"TypeName");
        lua_pushstring(L, RegisterTypeNames[counter_map[i-1].type]);
        lua_settable(L,-3);
        lua_pushstring(L,"Index");
        lua_pushunsigned(L,counter_map[i-1].index);
        lua_settable(L,-3);
        lua_settable(L,-3);
    }
    lua_settable(L,-3);
    lua_pushstring(L,"Events");
    lua_newtable(L);
    for(i=1;i<=perfmon_numArchEvents;i++)
    {
        optStringIndex = 0;
        optString[0] = '\0';
        lua_pushunsigned(L,i);
        lua_newtable(L);
        lua_pushstring(L,"Name");
        lua_pushstring(L,eventHash[i-1].name);
        lua_settable(L,-3);
        lua_pushstring(L,"ID");
        lua_pushunsigned(L,eventHash[i-1].eventId);
        lua_settable(L,-3);
        lua_pushstring(L,"UMask");
        lua_pushunsigned(L,eventHash[i-1].umask);
        lua_settable(L,-3);
        lua_pushstring(L,"Limit");
        lua_pushstring(L,eventHash[i-1].limit);
        lua_settable(L,-3);
        lua_pushstring(L,"Options");
        for(int j=1; j<NUM_EVENT_OPTIONS; j++)
        {
            if (eventHash[i-1].optionMask & REG_TYPE_MASK(j))
            {
                optStringIndex += sprintf(&(optString[optStringIndex]), "%s|", eventOptionTypeName[j]);
            }
        }
        optString[optStringIndex-1] = '\0';
        lua_pushstring(L,optString);
        lua_settable(L,-3);
        lua_settable(L,-3);
    }
    lua_settable(L,-3);
    return 1;
}

static int lua_likwid_getOnlineDevices(lua_State* L)
{
    int i;
    lua_newtable(L);
    for(i=0;i<=MAX_NUM_PCI_DEVICES;i++)
    {
        if (pci_devices[i].online)
        {
            lua_pushstring(L,pci_devices[i].likwid_name);
            lua_newtable(L);
            lua_pushstring(L, "Name");
            lua_pushstring(L,pci_devices[i].name);
            lua_settable(L,-3);
            lua_pushstring(L, "Path");
            lua_pushstring(L,pci_devices[i].path);
            lua_settable(L,-3);
            lua_pushstring(L, "Type");
            lua_pushstring(L,pci_types[pci_devices[i].type].name);
            lua_settable(L,-3);
            lua_pushstring(L, "TypeDescription");
            lua_pushstring(L,pci_types[pci_devices[i].type].desc);
            lua_settable(L,-3);
        }
        lua_settable(L,-3);
    }
    return 1;
}

static int lua_likwid_getNumaInfo(lua_State* L)
{
    uint32_t i,j;
    if (topology_isInitialized == 0)
    {
        topology_init();
        topology_isInitialized = 1;
        cpuinfo = get_cpuInfo();
        cputopo = get_cpuTopology();
    }
    if ((topology_isInitialized) && (cpuinfo == NULL))
    {
        cpuinfo = get_cpuInfo();
    }
    if ((topology_isInitialized) && (cputopo == NULL))
    {
        cputopo = get_cpuTopology();
    }
    if (numa_isInitialized == 0)
    {
        if (numa_init() == 0)
        {
            numa_isInitialized = 1;
            numainfo = get_numaTopology();
        }
        else
        {
            lua_newtable(L);
            lua_pushstring(L,"numberOfNodes");
            lua_pushunsigned(L,0);
            lua_settable(L,-3);
            lua_pushstring(L,"nodes");
            lua_newtable(L);
            lua_settable(L,-3);
            return 1;
        }
    }
    if ((numa_isInitialized) && (numainfo == NULL))
    {
        numainfo = get_numaTopology();
    }
    if (affinity_isInitialized == 0)
    {
        affinity_init();
        affinity_isInitialized = 1;
        affinity = get_affinityDomains();
    }
    if ((affinity_isInitialized) && (affinity == NULL))
    {
        affinity = get_affinityDomains();
    }
    lua_newtable(L);
    lua_pushstring(L,"numberOfNodes");
    lua_pushunsigned(L,numainfo->numberOfNodes);
    lua_settable(L,-3);

    lua_pushstring(L,"nodes");
    lua_newtable(L);
    for(i=0;i<numainfo->numberOfNodes;i++)
    {
        lua_pushinteger(L, i+1);
        lua_newtable(L);
        
        lua_pushstring(L,"id");
        lua_pushunsigned(L,numainfo->nodes[i].id);
        lua_settable(L,-3);
        lua_pushstring(L,"totalMemory");
        lua_pushunsigned(L,numainfo->nodes[i].totalMemory);
        lua_settable(L,-3);
        lua_pushstring(L,"freeMemory");
        lua_pushunsigned(L,numainfo->nodes[i].freeMemory);
        lua_settable(L,-3);
        lua_pushstring(L,"numberOfProcessors");
        lua_pushunsigned(L,numainfo->nodes[i].numberOfProcessors);
        lua_settable(L,-3);
        lua_pushstring(L,"numberOfDistances");
        lua_pushunsigned(L,numainfo->nodes[i].numberOfDistances);
        lua_settable(L,-3);
        
        lua_pushstring(L,"processors");
        lua_newtable(L);
        for(j=0;j<numainfo->nodes[i].numberOfProcessors;j++)
        {
            lua_pushunsigned(L,j+1);
            lua_pushunsigned(L,numainfo->nodes[i].processors[j]);
            lua_settable(L,-3);
        }
        lua_settable(L,-3);
        
        /*lua_pushstring(L,"processorsCompact");
        lua_newtable(L);
        for(j=0;j<numa->nodes[i].numberOfProcessors;j++)
        {
            lua_pushunsigned(L,j);
            lua_pushunsigned(L,numa->nodes[i].processorsCompact[j]);
            lua_settable(L,-3);
        }
        lua_settable(L,-3);*/
        
        lua_pushstring(L,"distances");
        lua_newtable(L);
        for(j=0;j<numainfo->nodes[i].numberOfDistances;j++)
        {
            lua_pushinteger(L,j+1);
            lua_newtable(L);
            lua_pushinteger(L,j);
            lua_pushunsigned(L,numainfo->nodes[i].distances[j]);
            lua_settable(L,-3);
            lua_settable(L,-3);
        }
        lua_settable(L,-3);
        
        lua_settable(L,-3);
    }
    lua_settable(L,-3);
    return 1;
}

static int lua_likwid_putNumaInfo(lua_State* L)
{
    if (numa_isInitialized)
    {
        numa_finalize();
        numa_isInitialized = 0;
        numainfo = NULL;
    }
    return 0;
}

static int lua_likwid_setMemInterleaved(lua_State* L)
{
    int ret;
    int nrThreads = luaL_checknumber(L,1);
    luaL_argcheck(L, nrThreads > 0, 1, "Thread count must be greater than 0");
    int cpus[nrThreads];
    if (!lua_istable(L, -1)) {
      lua_pushstring(L,"No table given as second argument");
      lua_error(L);
    }
    for (ret = 1; ret<=nrThreads; ret++)
    {
        lua_rawgeti(L,-1,ret);
        cpus[ret-1] = lua_tounsigned(L,-1);
        lua_pop(L,1);
    }
    numa_setInterleaved(cpus, nrThreads);
    return 0;
}

static int lua_likwid_getAffinityInfo(lua_State* L)
{
    int i,j;
    
    if (topology_isInitialized == 0)
    {
        topology_init();
        topology_isInitialized = 1;
        cpuinfo = get_cpuInfo();
        cputopo = get_cpuTopology();
    }
    if ((topology_isInitialized) && (cpuinfo == NULL))
    {
        cpuinfo = get_cpuInfo();
    }
    if ((topology_isInitialized) && (cputopo == NULL))
    {
        cputopo = get_cpuTopology();
    }
    if (numa_isInitialized == 0)
    {
        if (numa_init() == 0)
        {
            numa_isInitialized = 1;
            numainfo = get_numaTopology();
        }
    }
    if ((numa_isInitialized) && (numainfo == NULL))
    {
        numainfo = get_numaTopology();
    }
    if (affinity_isInitialized == 0)
    {
        affinity_init();
        affinity_isInitialized = 1;
        affinity = get_affinityDomains();
    }
    if ((affinity_isInitialized) && (affinity == NULL))
    {
        affinity = get_affinityDomains();
    }

    if (!affinity)
    {
        lua_pushstring(L,"Cannot initialize affinity groups");
        lua_error(L);
    }
    lua_newtable(L);
    lua_pushstring(L,"numberOfAffinityDomains");
    lua_pushunsigned(L,affinity->numberOfAffinityDomains);
    lua_settable(L,-3);
    lua_pushstring(L,"numberOfSocketDomains");
    lua_pushunsigned(L,affinity->numberOfSocketDomains);
    lua_settable(L,-3);
    lua_pushstring(L,"numberOfNumaDomains");
    lua_pushunsigned(L,affinity->numberOfNumaDomains);
    lua_settable(L,-3);
    lua_pushstring(L,"numberOfProcessorsPerSocket");
    lua_pushunsigned(L,affinity->numberOfProcessorsPerSocket);
    lua_settable(L,-3);
    lua_pushstring(L,"numberOfCacheDomains");
    lua_pushunsigned(L,affinity->numberOfCacheDomains);
    lua_settable(L,-3);
    lua_pushstring(L,"numberOfCoresPerCache");
    lua_pushunsigned(L,affinity->numberOfCoresPerCache);
    lua_settable(L,-3);
    lua_pushstring(L,"numberOfProcessorsPerCache");
    lua_pushunsigned(L,affinity->numberOfProcessorsPerCache);
    lua_settable(L,-3);
    lua_pushstring(L,"domains");
    lua_newtable(L);
    for(i=0;i<affinity->numberOfAffinityDomains;i++)
    {
        lua_pushunsigned(L, i+1);
        lua_newtable(L);
        lua_pushstring(L,"tag");
        lua_pushstring(L,bdata(affinity->domains[i].tag));
        lua_settable(L,-3);
        lua_pushstring(L,"numberOfProcessors");
        lua_pushunsigned(L,affinity->domains[i].numberOfProcessors);
        lua_settable(L,-3);
        lua_pushstring(L,"numberOfCores");
        lua_pushunsigned(L,affinity->domains[i].numberOfCores);
        lua_settable(L,-3);
        lua_pushstring(L,"processorList");
        lua_newtable(L);
        for(j=0;j<affinity->domains[i].numberOfProcessors;j++)
        {
            lua_pushunsigned(L,j+1);
            lua_pushunsigned(L,affinity->domains[i].processorList[j]);
            lua_settable(L,-3);
        }
        lua_settable(L,-3);
        lua_settable(L,-3);
    }
    lua_settable(L,-3);
    return 1;
}

static int lua_likwid_cpustr_to_cpulist(lua_State* L)
{
    int ret = 0;
    char* cpustr = (char *)luaL_checkstring(L, 1);
    int* cpulist = (int*) malloc(MAX_NUM_THREADS * sizeof(int));
    if (cpulist == NULL)
    {
        lua_pushstring(L,"Cannot allocate data for the CPU list");
        lua_error(L);
    }
    ret = cpustr_to_cpulist(cpustr, cpulist, MAX_NUM_THREADS);
    if (ret <= 0)
    {
        lua_pushstring(L,"Cannot parse cpustring");
        lua_error(L);
    }
    lua_pushnumber(L, ret);
    lua_newtable(L);
    for (int i=0;i<ret;i++)
    {
        lua_pushunsigned(L, i+1);
        lua_pushunsigned(L, cpulist[i]);
        lua_settable(L,-3);
    }
    free(cpulist);
    return 2;
}

static int lua_likwid_nodestr_to_nodelist(lua_State* L)
{
    int ret = 0;
    char* nodestr = (char *)luaL_checkstring(L, 1);
    int* nodelist = (int*) malloc(MAX_NUM_NODES * sizeof(int));
    if (nodelist == NULL)
    {
        lua_pushstring(L,"Cannot allocate data for the node list");
        lua_error(L);
    }
    ret = nodestr_to_nodelist(nodestr, nodelist, MAX_NUM_NODES);
    if (ret <= 0)
    {
        lua_pushstring(L,"Cannot parse node string");
        lua_error(L);
    }
    lua_pushnumber(L, ret);
    lua_newtable(L);
    for (int i=0;i<ret;i++)
    {
        lua_pushunsigned(L, i+1);
        lua_pushunsigned(L, nodelist[i]);
        lua_settable(L,-3);
    }
    free(nodelist);
    return 2;
}

static int lua_likwid_sockstr_to_socklist(lua_State* L)
{
    int ret = 0;
    char* sockstr = (char *)luaL_checkstring(L, 1);
    int* socklist = (int*) malloc(MAX_NUM_NODES * sizeof(int));
    if (socklist == NULL)
    {
        lua_pushstring(L,"Cannot allocate data for the socket list");
        lua_error(L);
    }
    ret = nodestr_to_nodelist(sockstr, socklist, MAX_NUM_NODES);
    if (ret <= 0)
    {
        lua_pushstring(L,"Cannot parse socket string");
        lua_error(L);
    }
    lua_pushnumber(L, ret);
    lua_newtable(L);
    for (int i=0;i<ret;i++)
    {
        lua_pushunsigned(L, i+1);
        lua_pushunsigned(L, socklist[i]);
        lua_settable(L,-3);
    }
    free(socklist);
    return 2;
}

static int lua_likwid_putAffinityInfo(lua_State* L)
{
    if (affinity_isInitialized)
    {
        affinity_finalize();
        affinity_isInitialized = 0;
        affinity = NULL;
    }
    return 0;
}

static int lua_likwid_getPowerInfo(lua_State* L)
{
    
    int i;
    if (topology_isInitialized == 0)
    {
        topology_init();
        topology_isInitialized = 1;
        cpuinfo = get_cpuInfo();
        cputopo = get_cpuTopology();
    }
    if ((topology_isInitialized) && (cpuinfo == NULL))
    {
        cpuinfo = get_cpuInfo();
    }
    if ((topology_isInitialized) && (cputopo == NULL))
    {
        cputopo = get_cpuTopology();
    }
    if (power_isInitialized == 0)
    {
        power_hasRAPL = power_init(0);
        if (power_hasRAPL)
        {
            power_isInitialized = 1;
            power = get_powerInfo();
        }
        else
        {
            return 0;
        }
    }


    lua_newtable(L);
    lua_pushstring(L,"hasRAPL");
    lua_pushboolean(L,power_hasRAPL);
    lua_settable(L,-3);
    lua_pushstring(L,"baseFrequency");
    lua_pushnumber(L,power->baseFrequency);
    lua_settable(L,-3);
    lua_pushstring(L,"minFrequency");
    lua_pushnumber(L,power->minFrequency);
    lua_settable(L,-3);
    lua_pushstring(L,"powerUnit");
    lua_pushnumber(L,power->powerUnit);
    lua_settable(L,-3);
    lua_pushstring(L,"timeUnit");
    lua_pushnumber(L,power->timeUnit);
    lua_settable(L,-3);
    
    lua_pushstring(L,"turbo");
    lua_newtable(L);
    lua_pushstring(L,"numSteps");
    lua_pushunsigned(L,power->turbo.numSteps);
    lua_settable(L,-3);
    lua_pushstring(L,"steps");
    lua_newtable(L);
    for(i=0;i<power->turbo.numSteps;i++)
    {
        lua_pushunsigned(L,i+1);
        lua_pushnumber(L,power->turbo.steps[i]);
        lua_settable(L,-3);
    }
    lua_settable(L,-3);
    lua_settable(L,-3);

    lua_pushstring(L,"domains");
    lua_newtable(L);
    for(i=0;i<NUM_POWER_DOMAINS;i++)
    {
        lua_pushstring(L,power_names[i]);
        lua_newtable(L);

        lua_pushstring(L, "ID");
        lua_pushnumber(L, power->domains[i].type);
        lua_settable(L,-3);
        lua_pushstring(L, "energyUnit");
        lua_pushnumber(L, power->domains[i].energyUnit);
        lua_settable(L,-3);
        lua_pushstring(L,"supportStatus");
        if (power->domains[i].supportFlags & POWER_DOMAIN_SUPPORT_STATUS)
        {
            lua_pushboolean(L, 1);
        }
        else
        {
            lua_pushboolean(L, 0);
        }
        lua_settable(L,-3);
        lua_pushstring(L,"supportPerf");
        if (power->domains[i].supportFlags & POWER_DOMAIN_SUPPORT_PERF)
        {
            lua_pushboolean(L, 1);
        }
        else
        {
            lua_pushboolean(L, 0);
        }
        lua_settable(L,-3);
        lua_pushstring(L,"supportPolicy");
        if (power->domains[i].supportFlags & POWER_DOMAIN_SUPPORT_POLICY)
        {
            lua_pushboolean(L, 1);
        }
        else
        {
            lua_pushboolean(L, 0);
        }
        lua_settable(L,-3);
        lua_pushstring(L,"supportLimit");
        if (power->domains[i].supportFlags & POWER_DOMAIN_SUPPORT_LIMIT)
        {
            lua_pushboolean(L, 1);
        }
        else
        {
            lua_pushboolean(L, 0);
        }
        lua_settable(L,-3);
        if (power->domains[i].supportFlags & POWER_DOMAIN_SUPPORT_INFO)
        {
            lua_pushstring(L,"supportInfo");
            lua_pushboolean(L, 1);
            lua_settable(L,-3);
            lua_pushstring(L,"tdp");
            lua_pushnumber(L, power->domains[i].tdp);
            lua_settable(L,-3);
            lua_pushstring(L,"minPower");
            lua_pushnumber(L, power->domains[i].minPower);
            lua_settable(L,-3);
            lua_pushstring(L,"maxPower");
            lua_pushnumber(L, power->domains[i].maxPower);
            lua_settable(L,-3);
            lua_pushstring(L,"maxTimeWindow");
            lua_pushnumber(L, power->domains[i].maxTimeWindow);
            lua_settable(L,-3);
        }
        else
        {
            lua_pushstring(L,"supportInfo");
            lua_pushboolean(L, 0);
            lua_settable(L,-3);
        }

        lua_settable(L,-3);
    }
    lua_settable(L,-3);
    

    return 1;
}

static int lua_likwid_putPowerInfo(lua_State* L)
{
    if (power_isInitialized)
    {
        power_finalize();
        power_isInitialized = 0;
        power = NULL;
    }
    return 0;
}

static int lua_likwid_startPower(lua_State* L)
{
    PowerData pwrdata;
    int cpuId = lua_tonumber(L,1);
    luaL_argcheck(L, cpuId >= 0, 1, "CPU ID must be greater than 0");
    PowerType type = (PowerType) lua_tounsigned(L,2);
    luaL_argcheck(L, type >= PKG+1 && type <= DRAM+1, 2, "Type not valid");
    power_start(&pwrdata, cpuId, type-1);
    lua_pushnumber(L,pwrdata.before);
    return 1;
}

static int lua_likwid_stopPower(lua_State* L)
{
    PowerData pwrdata;
    int cpuId = lua_tonumber(L,1);
    luaL_argcheck(L, cpuId >= 0, 1, "CPU ID must be greater than 0");
    PowerType type = (PowerType) lua_tounsigned(L,2);
    luaL_argcheck(L, type >= PKG+1 && type <= DRAM+1, 2, "Type not valid");
    power_stop(&pwrdata, cpuId, type-1);
    lua_pushnumber(L,pwrdata.after);
    return 1;
}

static int lua_likwid_printEnergy(lua_State* L)
{
    PowerData pwrdata;
    pwrdata.before = lua_tonumber(L,1);
    pwrdata.after = lua_tonumber(L,2);
    pwrdata.domain = lua_tonumber(L,3);
    lua_pushnumber(L,power_printEnergy(&pwrdata));
    return 1;
}

static int lua_likwid_power_limitGet(lua_State* L)
{
    int err;
    int cpuId = lua_tonumber(L,1);
    int domain = lua_tonumber(L,2);
    double power = 0.0;
    double time = 0.0;
    err = power_limitGet(cpuId, domain, &power, &time);
    if (err < 0)
    {
        lua_pushnumber(L,err);
        return 1;
    }
    lua_pushnumber(L,power);
    lua_pushnumber(L,time);
    return 2;
}

static int lua_likwid_power_limitSet(lua_State* L)
{
    int cpuId = lua_tonumber(L,1);
    int domain = lua_tonumber(L,2);
    double power = lua_tonumber(L,3);
    double time = lua_tonumber(L,4);
    int clamp  = lua_tonumber(L,5);
    lua_pushinteger(L, power_limitSet(cpuId, domain, power, time, clamp));
    return 1;
}

static int lua_likwid_power_limitState(lua_State* L)
{
    int cpuId = lua_tonumber(L,1);
    int domain = lua_tonumber(L,2);
    lua_pushnumber(L,power_limitState(cpuId, domain));
    return 1;
}

static int lua_likwid_getCpuClock(lua_State* L)
{
    if (timer_isInitialized == 0)
    {
        timer_init();
        timer_isInitialized = 1;
    }
    lua_pushnumber(L,timer_getCpuClock());
    return 1;
}

static int lua_sleep(lua_State* L)
{
    lua_pushnumber(L, timer_sleep(lua_tounsigned(L,-1)));
    return 1;
}

static int lua_likwid_startClock(lua_State* L)
{
    TimerData timer;
    double value;
    if (timer_isInitialized == 0)
    {
        timer_init();
        timer_isInitialized = 1;
    }
    timer_start(&timer);
    value = (double)timer.start.int64;
    lua_pushnumber(L, value);
    return 1;
}

static int lua_likwid_stopClock(lua_State* L)
{
    TimerData timer;
    double value;
    if (timer_isInitialized == 0)
    {
        timer_init();
        timer_isInitialized = 1;
    }
    timer_stop(&timer);
    value = (double)timer.stop.int64;
    lua_pushnumber(L, value);
    return 1;
}

static int lua_likwid_getClockCycles(lua_State* L)
{
    TimerData timer;
    double start, stop;
    start = lua_tonumber(L,1);
    stop = lua_tonumber(L,2);
    timer.start.int64 = (uint64_t)start;
    timer.stop.int64 = (uint64_t)stop;
    if (timer_isInitialized == 0)
    {
        timer_init();
        timer_isInitialized = 1;
    }
    lua_pushnumber(L, (double)timer_printCycles(&timer));
    return 1;
}

static int lua_likwid_getClock(lua_State* L)
{
    TimerData timer;
    double runtime, start, stop;
    if (timer_isInitialized == 0)
    {
        timer_init();
        timer_isInitialized = 1;
    }
    start = lua_tonumber(L,1);
    stop = lua_tonumber(L,2);
    timer.start.int64 = (uint64_t)start;
    timer.stop.int64 = (uint64_t)stop;
    runtime = timer_print(&timer);
    lua_pushnumber(L, runtime);
    return 1;
}

static int lua_likwid_initTemp(lua_State* L)
{
    int cpuid = lua_tounsigned(L,-1);
    thermal_init(cpuid);
    return 0;
}

static int lua_likwid_readTemp(lua_State* L)
{
    int cpuid = lua_tounsigned(L,-1);
    uint32_t data;
    
    if (thermal_read(cpuid, &data)) {
        lua_pushstring(L,"Cannot read thermal data");
        lua_error(L);
    }
    lua_pushnumber(L, data);
    return 1;
}


static volatile int recv_sigint = 0;

static void signal_catcher(int signo) 
{
    if (signo == SIGINT)
    {
        recv_sigint++;
    }
    return;
}

static int lua_likwid_catch_signal(lua_State* L)
{
    signal(SIGINT,signal_catcher);
    return 0;
}

static int lua_likwid_return_signal_state(lua_State* L)
{
    lua_pushnumber(L, recv_sigint);
    return 1;
}

void parse(char *line, char **argv)
{
     while (*line != '\0') {       /* if not the end of line ....... */ 
          while (*line == ' ' || *line == '\t' || *line == '\n')
               *line++ = '\0';     /* replace white spaces with 0    */
          *argv++ = line;          /* save the argument position     */
          while (*line != '\0' && *line != ' ' && 
                 *line != '\t' && *line != '\n') 
               line++;             /* skip the argument until ...    */
     }
     *argv = '\0';                 /* mark the end of argument list  */
}

static volatile int program_running = 0;

static void catch_sigchild(int signo) {
    program_running = 0;
}

static int lua_likwid_startProgram(lua_State* L)
{
    pid_t pid, ppid;
    int status;
    char *exec;
    char  *argv[4096];
    exec = (char *)luaL_checkstring(L, 1);
    int nrThreads = luaL_checknumber(L,2);
    int cpus[MAX_NUM_THREADS];
    cpu_set_t cpuset;
    if (nrThreads > 0)
    {
        if (!lua_istable(L, -1)) {
          lua_pushstring(L,"No table given as second argument");
          lua_error(L);
        }
        for (status = 1; status<=nrThreads; status++)
        {
            lua_rawgeti(L,-1,status);
            cpus[status-1] = lua_tounsigned(L,-1);
            lua_pop(L,1);
        }
    }
    else
    {
        for (nrThreads = 0; nrThreads < cpuid_topology.numHWThreads; nrThreads++)
            cpus[nrThreads] = cpuid_topology.threadPool[nrThreads].apicId;
        nrThreads = cpuid_topology.numHWThreads;
    }
    parse(exec, argv);
    ppid = getpid();
    program_running = 1;
    pid = fork();
    if (pid < 0)
    {
        return 0;
    }
    else if ( pid == 0)
    {
        if (nrThreads > 0)
        {
            affinity_pinProcesses(nrThreads, cpus);
        }
        timer_sleep(10);
        status = execvp(*argv, argv);
        if (status < 0)
        {
            kill(ppid, SIGCHLD);
        }
        return 0;
    }
    else
    {
        signal(SIGCHLD, catch_sigchild);
        lua_pushnumber(L, pid);
    }
    return 1;
}
static int lua_likwid_checkProgram(lua_State* L)
{
    lua_pushboolean(L, program_running);
    return 1;
}

static int lua_likwid_killProgram(lua_State* L)
{
    pid_t pid = lua_tonumber(L, 1);
    kill(pid, SIGTERM);
    program_running = 0;
    return 0;
}


static int lua_likwid_memSweep(lua_State* L)
{
    int i;
    int nrThreads = luaL_checknumber(L,1);
    luaL_argcheck(L, nrThreads > 0, 1, "Thread count must be greater than 0");
    int cpus[nrThreads];
    if (!lua_istable(L, -1)) {
      lua_pushstring(L,"No table given as second argument");
      lua_error(L);
    }
    for (i = 1; i <= nrThreads; i++)
    {
        lua_rawgeti(L,-1,i);
        cpus[i-1] = lua_tounsigned(L,-1);
        lua_pop(L,1);
    }
    memsweep_threadGroup(cpus, nrThreads);
    return 0;
}

static int lua_likwid_memSweepDomain(lua_State* L)
{
    int domain = luaL_checknumber(L,1);
    luaL_argcheck(L, domain >= 0, 1, "Domain ID must be greater or equal 0");
    memsweep_domain(domain);
    return 0;
}

static int lua_likwid_pinProcess(lua_State* L)
{
    int cpuID = luaL_checknumber(L,-2);
    int silent = luaL_checknumber(L,-1);
    luaL_argcheck(L, cpuID >= 0, 1, "CPU ID must be greater or equal 0");
    if (affinity_isInitialized == 0)
    {
        affinity_init();
        affinity_isInitialized = 1;
        affinity = get_affinityDomains();
    }
    affinity_pinProcess(cpuID);
    if (!silent)
    {
#ifdef COLOR
            color_on(BRIGHT, COLOR);
#endif
            printf("[likwid-pin] Main PID -> core %d - OK",  cpuID);
#ifdef COLOR
            color_reset();
#endif
            printf("\n");
    }
    return 0;
}

static int lua_likwid_setenv(lua_State* L)
{
    const char* element = (const char*)luaL_checkstring(L, -2);
    const char* value = (const char*)luaL_checkstring(L, -1);
    setenv(element, value, 1);
    return 0;
}

static int lua_likwid_getpid(lua_State* L)
{
    lua_pushunsigned(L,getpid());
    return 1;
}

static int lua_likwid_setVerbosity(lua_State* L)
{
    int verbosity = lua_tointeger(L,-1);
    luaL_argcheck(L, (verbosity >= 0 && verbosity <= DEBUGLEV_DEVELOP), -1, 
                "Verbosity must be between 0 (only errors) and 3 (developer)");
    perfmon_verbosity = verbosity;
    return 0;
}

static int lua_likwid_access(lua_State* L)
{
    int flags = 0;
    const char* file = (const char*)luaL_checkstring(L, 1);
    const char* perm = (const char*)luaL_checkstring(L, 2);
    if (!perm)
    {
        flags = F_OK;
    }
    else
    {
        for (int i=0;i<strlen(perm);i++)
        {
            if (perm[i] == 'r') {
                flags |= R_OK;
            } else if (perm[i] == 'w') {
                flags |= W_OK;
            } else if (perm[i] == 'x') {
                flags |= X_OK;
            } else if (perm[i] == 'e') {
                flags |= F_OK;
            }
        }
    }
    if (file)
    {
        lua_pushinteger(L, access(file, flags));
        return 1;
    }
    lua_pushinteger(L, -1);
    return 1;
}

static int lua_likwid_markerInit(lua_State* L)
{
    likwid_markerInit();
    return 0;
}

static int lua_likwid_markerThreadInit(lua_State* L)
{
    likwid_markerThreadInit();
    return 0;
}

static int lua_likwid_markerClose(lua_State* L)
{
    likwid_markerClose();
    return 0;
}

static int lua_likwid_markerNext(lua_State* L)
{
    likwid_markerNextGroup();
    return 0;
}

static int lua_likwid_registerRegion(lua_State* L)
{
    const char* tag = (const char*)luaL_checkstring(L, -1);
    lua_pushinteger(L, likwid_markerRegisterRegion(tag));
    return 1;
}

static int lua_likwid_startRegion(lua_State* L)
{
    const char* tag = (const char*)luaL_checkstring(L, -1);
    lua_pushinteger(L, likwid_markerStartRegion(tag));
    return 1;
}

static int lua_likwid_stopRegion(lua_State* L)
{
    const char* tag = (const char*)luaL_checkstring(L, -1);
    lua_pushinteger(L, likwid_markerStopRegion(tag));
    return 1;
}

static int lua_likwid_getRegion(lua_State* L)
{
    int i = 0;
    const char* tag = (const char*)luaL_checkstring(L, -2);
    int nr_events = perfmon_getNumberOfEvents(perfmon_getIdOfActiveGroup());
    double* events = NULL;
    double time = 0.0;
    int count = 0;
    
    events = (double*) malloc(nr_events * sizeof(double));
    if (events == NULL)
    {
        lua_pushstring(L,"Cannot allocate memory for event data\n");
        lua_error(L);
    }
    for (i = 0; i < nr_events; i++)
    {
        events[i] = 0.0;
    }
    likwid_markerGetRegion(tag, &nr_events, events, &time, &count);
    
    lua_pushinteger(L, nr_events);
    lua_newtable(L);
    for (i=0;i<nr_events;i++)
    {
        lua_pushinteger(L, i+1);
        lua_pushnumber(L, events[i]);
        lua_settable(L, -3);
    }
    lua_pushnumber(L, time);
    lua_pushinteger(L, count);
    free(events);
    return 4;
}

static int lua_likwid_cpuFeatures_init(lua_State* L)
{
    cpuFeatures_init();
    return 0;
}

static int lua_likwid_cpuFeatures_print(lua_State* L)
{
    int cpu = lua_tointeger(L,-1);
    cpuFeatures_print(cpu);
    return 0;
}

static int lua_likwid_cpuFeatures_get(lua_State* L)
{
    int cpu = lua_tointeger(L,-2);
    CpuFeature feature = lua_tointeger(L,-1);
    lua_pushinteger(L, cpuFeatures_get(cpu, feature));
    return 1;
}

static int lua_likwid_cpuFeatures_name(lua_State* L)
{
    char* name = NULL;
    CpuFeature feature = lua_tounsigned(L,-1);
    name = cpuFeatures_name(feature);
    if (name != NULL)
    {
        lua_pushstring(L, name);
        return 1;
    }
    return 0;
}

static int lua_likwid_cpuFeatures_enable(lua_State* L)
{
    int cpu = lua_tointeger(L,-3);
    CpuFeature feature = lua_tointeger(L,-2);
    int verbose = lua_tointeger(L,-1);
    lua_pushinteger(L, cpuFeatures_enable(cpu, feature, verbose));
    return 1;
}

static int lua_likwid_cpuFeatures_disable(lua_State* L)
{
    int cpu = lua_tointeger(L,-3);
    CpuFeature feature = lua_tointeger(L,-2);
    int verbose = lua_tointeger(L,-1);
    lua_pushinteger(L, cpuFeatures_disable(cpu, feature, verbose));
    return 1;
}

int __attribute__ ((visibility ("default") )) luaopen_liblikwid(lua_State* L){
    // Configuration functions
    lua_register(L, "likwid_getConfiguration", lua_likwid_getConfiguration);
    lua_register(L, "likwid_putConfiguration", lua_likwid_putConfiguration);
    // Perfmon functions
    //lua_register(L, "accessClient_setaccessmode",lua_accessClient_setaccessmode);
    lua_register(L, "likwid_setAccessClientMode",lua_likwid_setAccessMode);
    lua_register(L, "likwid_init",lua_likwid_init);
    lua_register(L, "likwid_addEventSet", lua_likwid_addEventSet);
    lua_register(L, "likwid_setupCounters",lua_likwid_setupCounters);
    lua_register(L, "likwid_startCounters",lua_likwid_startCounters);
    lua_register(L, "likwid_stopCounters",lua_likwid_stopCounters);
    lua_register(L, "likwid_readCounters",lua_likwid_readCounters);
    lua_register(L, "likwid_switchGroup",lua_likwid_switchGroup);
    lua_register(L, "likwid_finalize",lua_likwid_finalize);
    lua_register(L, "likwid_getEventsAndCounters", lua_likwid_getEventsAndCounters);
    // Perfmon results functions
    lua_register(L, "likwid_getResult",lua_likwid_getResult);
    lua_register(L, "likwid_getNumberOfGroups",lua_likwid_getNumberOfGroups);
    lua_register(L, "likwid_getRuntimeOfGroup", lua_likwid_getRuntimeOfGroup);
    lua_register(L, "likwid_getIdOfActiveGroup",lua_likwid_getIdOfActiveGroup);
    lua_register(L, "likwid_getNumberOfEvents",lua_likwid_getNumberOfEvents);
    lua_register(L, "likwid_getNumberOfThreads",lua_likwid_getNumberOfThreads);
    // Topology functions
    lua_register(L, "likwid_getCpuInfo",lua_likwid_getCpuInfo);
    lua_register(L, "likwid_getCpuTopology",lua_likwid_getCpuTopology);
    lua_register(L, "likwid_putTopology",lua_likwid_putTopology);
    lua_register(L, "likwid_getNumaInfo",lua_likwid_getNumaInfo);
    lua_register(L, "likwid_putNumaInfo",lua_likwid_putNumaInfo);
    lua_register(L, "likwid_setMemInterleaved", lua_likwid_setMemInterleaved);
    lua_register(L, "likwid_getAffinityInfo",lua_likwid_getAffinityInfo);
    lua_register(L, "likwid_putAffinityInfo",lua_likwid_putAffinityInfo);
    lua_register(L, "likwid_getPowerInfo",lua_likwid_getPowerInfo);
    lua_register(L, "likwid_putPowerInfo",lua_likwid_putPowerInfo);
    lua_register(L, "likwid_getOnlineDevices", lua_likwid_getOnlineDevices);
    lua_register(L, "likwid_printSupportedCPUs", lua_likwid_printSupportedCPUs);
    // CPU string parse functions
    lua_register(L, "likwid_cpustr_to_cpulist",lua_likwid_cpustr_to_cpulist);
    lua_register(L, "likwid_nodestr_to_nodelist",lua_likwid_nodestr_to_nodelist);
    lua_register(L, "likwid_sockstr_to_socklist",lua_likwid_sockstr_to_socklist);
    // Timer functions
    lua_register(L, "likwid_getCpuClock",lua_likwid_getCpuClock);
    lua_register(L, "likwid_startClock",lua_likwid_startClock);
    lua_register(L, "likwid_stopClock",lua_likwid_stopClock);
    lua_register(L, "likwid_getClockCycles",lua_likwid_getClockCycles);
    lua_register(L, "likwid_getClock",lua_likwid_getClock);
    lua_register(L, "sleep",lua_sleep);
    // Power functions
    lua_register(L, "likwid_startPower",lua_likwid_startPower);
    lua_register(L, "likwid_stopPower",lua_likwid_stopPower);
    lua_register(L, "likwid_printEnergy",lua_likwid_printEnergy);
    lua_register(L, "likwid_powerLimitGet",lua_likwid_power_limitGet);
    lua_register(L, "likwid_powerLimitSet",lua_likwid_power_limitSet);
    lua_register(L, "likwid_powerLimitState",lua_likwid_power_limitState);
    // Temperature functions
    lua_register(L, "likwid_initTemp",lua_likwid_initTemp);
    lua_register(L, "likwid_readTemp",lua_likwid_readTemp);
    // MemSweep functions
    lua_register(L, "likwid_memSweep", lua_likwid_memSweep);
    lua_register(L, "likwid_memSweepDomain", lua_likwid_memSweepDomain);
    // Pinning functions
    lua_register(L, "likwid_pinProcess", lua_likwid_pinProcess);
    // Helper functions
    lua_register(L, "likwid_setenv", lua_likwid_setenv);
    lua_register(L, "likwid_getpid", lua_likwid_getpid);
    lua_register(L, "likwid_access", lua_likwid_access);
    lua_register(L, "likwid_startProgram", lua_likwid_startProgram);
    lua_register(L, "likwid_checkProgram", lua_likwid_checkProgram);
    lua_register(L, "likwid_killProgram", lua_likwid_killProgram);
    // Verbosity functions
    lua_register(L, "likwid_setVerbosity", lua_likwid_setVerbosity);
    lua_register(L, "likwid_catchSignal", lua_likwid_catch_signal);
    lua_register(L, "likwid_getSignalState", lua_likwid_return_signal_state);
    // Marker API functions
    lua_register(L, "likwid_markerInit", lua_likwid_markerInit);
    lua_register(L, "likwid_markerThreadInit", lua_likwid_markerThreadInit);
    lua_register(L, "likwid_markerNextGroup", lua_likwid_markerNext);
    lua_register(L, "likwid_markerClose", lua_likwid_markerClose);
    lua_register(L, "likwid_registerRegion", lua_likwid_registerRegion);
    lua_register(L, "likwid_startRegion", lua_likwid_startRegion);
    lua_register(L, "likwid_stopRegion", lua_likwid_stopRegion);
    lua_register(L, "likwid_getRegion", lua_likwid_getRegion);
    // CPU feature manipulation functions
    lua_register(L, "likwid_cpuFeaturesInit", lua_likwid_cpuFeatures_init);
    lua_register(L, "likwid_cpuFeaturesGet", lua_likwid_cpuFeatures_get);
    lua_register(L, "likwid_cpuFeaturesEnable", lua_likwid_cpuFeatures_enable);
    lua_register(L, "likwid_cpuFeaturesDisable", lua_likwid_cpuFeatures_disable);
#ifdef __MIC__
    if (setuid(0) < 0)
    {
        printf("Cannot setuid to root\n");
    }
    if (seteuid(0) < 0)
    {
        printf("Cannot seteuid to root\n");
    }
#endif
    return 0;
}

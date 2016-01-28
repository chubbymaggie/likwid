#!<INSTALLED_BINPREFIX>/likwid-lua
--[[
 * =======================================================================================
 *
 *      Filename:  likwid-features.lua
 *
 *      Description:  A application to retrieve and manipulate CPU features.
 *
 *      Version:   4.0
 *      Released:  28.04.2015
 *
 *      Author:   Thomas Roehl (tr), thomas.roehl@gmail.com
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
]]
package.path = '<INSTALLED_PREFIX>/share/lua/?.lua;' .. package.path

local likwid = require("likwid")

function version()
    print(string.format("likwid-features --  Version %d.%d",likwid.version,likwid.release))
end

function usage()
    version()
    print("A tool list and modify the states of CPU features.\n")
    print("Options:")
    print("-h, --help\t\t Help message")
    print("-v, --version\t\t Version information")
    print("-a, --all\t\t List all available features")
    print("-l, --list\t\t List features and state for given CPUs")
    print("-c, --cpus <list>\t Perform operations on given CPUs")
    print("-e, --enable <list>\t List of features that should be enabled")
    print("-d, --disable <list>\t List of features that should be disabled")
    print()
    print("Currently modifiable features:")
    print("HW_PREFETCHER, CL_PREFETCHER, DCU_PREFETCHER, IP_PREFETCHER")
end

if #arg == 0 then
    usage()
    os.exit(0)
end

listFeatures = false
num_cpus = 0
cpulist = {}
enableList = {}
disableList = {}
skipList = {}

for opt,arg in likwid.getopt(arg, {"h","v","l","c:","e:","d:","a","help","version","list", "enable:", "disable:","all", "cpus:"}) do
    if (type(arg) == "string") then
        local s,e = arg:find("-");
        if s == 1 then
            print(string.format("Argmument %s to option -%s starts with invalid character -.", arg, opt))
            print("Did you forget an argument to an option?")
            os.exit(1)
        end
    end
    if opt == "h" or opt == "help" then
        usage()
        os.exit(0)
    elseif opt == "v" or opt == "version" then
        version()
        os.exit(0)
    elseif opt == "c" or opt == "cpus"then
        num_cpus, cpulist = likwid.cpustr_to_cpulist(arg)
    elseif opt == "l" or opt == "list" then
        listFeatures = true
    elseif opt == "a" or opt == "all" then
        print("Available features:")
        for i=0,likwid.tablelength(likwid.cpuFeatures)-1 do
            print(string.format("\t%s",likwid.cpuFeatures[i]))
        end
        os.exit(0)
    elseif opt == "e" or opt == "enable" then
        local tmp = likwid.stringsplit(arg, ",")
        for i, f in pairs(tmp) do
            for i=0,likwid.tablelength(likwid.cpuFeatures)-1 do
                if likwid.cpuFeatures[i] == f then
                    table.insert(enableList, i)
                end
            end
        end
    elseif opt == "d" or opt == "disable" then
        local tmp = likwid.stringsplit(arg, ",")
        for i, f in pairs(tmp) do
            for i=0,likwid.tablelength(likwid.cpuFeatures)-1 do
                if likwid.cpuFeatures[i] == f then
                    table.insert(disableList, i)
                end
            end
        end
    elseif opt == "?" then
        print("Invalid commandline option -"..arg)
        os.exit(1)
    elseif opt == "!" then
        print("Option requires an argument")
        os.exit(1)
    end
end

likwid.initCpuFeatures()

if listFeatures and #cpulist > 0 then
    local str = "Feature"..string.rep(" ",string.len("BRANCH_TRACE_STORAGE")-string.len("Feature")+2)
    for j, c in pairs(cpulist) do
        str = str..string.format("CPU %d\t",c)
    end
    print(str)
    str = ""
    for i=0,likwid.tablelength(likwid.cpuFeatures)-1 do
        str = likwid.cpuFeatures[i]..string.rep(" ",string.len("BRANCH_TRACE_STORAGE")-string.len(likwid.cpuFeatures[i])+2)
        for j, c in pairs(cpulist) do
            if (likwid.getCpuFeatures(c, i) == 1) then
                str = str .. "on\t"
            else
                str = str .. "off\t"
            end
        end
        print(str)
    end
elseif #cpulist == 0 then
    print("Need CPU to list current feature state")
    os.exit(1)
end

if #enableList > 0 and #disableList > 0 then
    for i,e in pairs(enableList) do
        for j, d in pairs(disableList) do
            if (e == d) then
                print(string.format("Feature %s is in enable and disable list, doing nothing for feature", e))
                table.insert(skipList, e)
            end
        end
    end
    for i, s in pairs(skipList) do
        for j, e in pairs(enableList) do
            if (s == e) then table.remove(enableList, j) end
        end
        for j, e in pairs(disableList) do
            if (s == e) then table.remove(disableList, j) end
        end
    end
end

if #enableList > 0 then
    for i, c in pairs(cpulist) do
        for j, f in pairs(enableList) do
            local ret = likwid.enableCpuFeatures(c, f, 1)
            if ret == 0 then
                print(string.format("Enabled %s", likwid.cpuFeatures[f]))
            else
                print(string.format("Failed %s", likwid.cpuFeatures[f]))
            end
        end
    end
end
if #disableList > 0 then
    for i, c in pairs(cpulist) do
        for j, f in pairs(disableList) do
            local ret = likwid.disableCpuFeatures(c, f, 1)
            if ret == 0 then
                print(string.format("Disabled %s", likwid.cpuFeatures[f]))
            else
                print(string.format("Failed %s", likwid.cpuFeatures[f]))
            end
        end
    end
end

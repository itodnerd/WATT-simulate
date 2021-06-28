#!/bin/python3

import matplotlib.pyplot as plt
import os, shutil, random, time, json, csv
from joblib import Parallel, delayed
import pandas


def getWorker(zugriff):
    return int(zugriff.split(" ")[0])

def getCommand(zugriff):
    return zugriff.split(":")[0].split(" ", 1)[1]

def getPid(zugriff): # Remove /n at end
    return int(zugriff.split(": ")[1])

def evictMinPage(ram):
    return min(ram, key=ram.get)

def randomFindPageToEvict(ram: dict):
    return random.choice(list(ram))

# First Try:
# Count every SWIP to pageId as valid access

def getOnlySwip(zugriffe):
    return list(filter(lambda x: "Resolving Swip" in x, zugriffe))

def getOnlyHotSwip(zugriffe):
    return list(filter(lambda x: "Hot" in x, zugriffe))
def getOnlyColdSwip(zugriffe):
    return list(filter(lambda x: "cold" in x, zugriffe))
def getOnlyCoolSwip(zugriffe):
    return list(filter(lambda x: "cool" in x, zugriffe))

def executeStrategy(pidAndNext, ramSize, flagFunction, evictFinder, heatUp= 0):
    currentRam = {}
    pageMisses = 0
    for pos in range(0, len(pidAndNext)):
        if pos == heatUp: # Heatup, ignore previous accesses
            pageMisses = 0
            
        (pid, nextZugriff) = pidAndNext[pos]
        flag = flagFunction([pos, nextZugriff])
        if pid in currentRam:
            currentRam[pid] = flag
        else:
            if len(currentRam) < ramSize:
                currentRam[pid] = flag
            else:
                evictMe = evictFinder(currentRam)

                del currentRam[evictMe]

                assert(len(currentRam) < ramSize)
                currentRam[pid] = flag
            pageMisses +=1
    return pageMisses

def belady(pidAndNext, ramSize, heatUp=0, opt=False, minMiss=0, maxHit=0):
    global lastHit, lastMiss
    if not opt:
        return executeStrategy(pidAndNext, ramSize, lambda x: x[1], evictMinPage, heatUp=heatUp)
    if (maxHit == lastHit):
        assert(minMiss == lastMiss)
        return (minMiss, maxHit)
    (lastMiss, lastHit) = executeStrategy(pidAndNext, ramSize, lambda x: x[1], evictMinPage, heatUp=heatUp)
    return (lastMiss, lastHit)

def lru(pidAndNext, ramSize, heatUp=0):
    return executeStrategy(pidAndNext, ramSize, lambda x: x[0], evictMinPage, heatUp=heatUp)

def ran(pidAndNext, ramSize, heatUp=0):
    return executeStrategy(pidAndNext, ramSize, lambda x: 0, randomFindPageToEvict, heatUp=heatUp)

def lruStack(pidAndNext, heatUp=0):
    stack = [] # The stack
    stackDist = {} # Fast access to counter of stack depth x
    stackContains = {} # fast acces to check, if element was already loaded
    for pos in range(0, len(pidAndNext)):
        if pos == heatUp: # Heatup, ignore previous accesses
            stackDist = {}
        (pid, _ ) = pidAndNext[pos]
        if pid in stackContains:
            depth = stack.index(pid)
            if depth in stackDist:
                stackDist[depth] = stackDist[depth] + 1
            else:
                stackDist[depth] = 1
            stack.remove(pid)
        else:
            if -1 in stackDist:
                stackDist[-1] = stackDist[-1] +1
            else:
                stackDist[-1] = 1
        stack.insert(0, pid)
        stackContains[pid]=0
    return stackDist

def evalStack(stack, ramSize):
    missrate = 0
    for pos in stack:
        value = stack[pos]
        if not (pos < ramSize and pos >= 0):
            missrate += value
    return missrate


def getNextAcces(pids, pos):
    pid = pids[pos]
    try:
        return len(pids) - pids.index(pid, pos+1)
    except ValueError:
        return -1


def pidAndNextAccessOld(zugriffe): # Watch out: nextAccess on reversed list => min gives latest access
    pids = [getPid(x) for x in zugriffe]
    return list(map(lambda x: (pids[x], getNextAcces(pids, x)), range(0, len(pids))))

def pidAndNextAccess(zugriffe: list): # Watch out: nextAccess on reversed list => min gives latest access
    length = len(zugriffe)
    lastUse = {}
    pidAndAccess = []
    zugriffe.reverse()
    for pos, line in enumerate(list(zugriffe)):
        pid = getPid(line)
        if pid in lastUse:
            pidAndAccess.append((pid, lastUse[pid]))
            lastUse[pid] = pos
        else:
            pidAndAccess.append((pid, -1))
            lastUse[pid] = pos
    pidAndAccess.reverse()
    zugriffe.reverse()
    return pidAndAccess


def getSwips(file, elements, all=False):
    datei = open(file, "r")
    zugriffe = datei.readlines()
    if all:
        return getOnlySwip(zugriffe)
    swips = getOnlySwip(zugriffe[elements])

    for counter in range(1, 100):
        swips += getOnlySwip(zugriffe[elements*counter:elements*(counter+1)])
        if len(swips) > elements:
            return swips[:elements]

def runner(pidAndNext, size):
    return ran(pidAndNext, size)


def generateCSV(zugriffe, name, heatUp=0, all=False, quick=False):
    global lastHit, lastMiss
    elements = len(zugriffe) - heatUp

    pre = time.time()

    print("PidAndNextAccessPrep")
    pidAndNext = pidAndNextAccess(zugriffe)

    range0 = 3
    range1 = 20
    range2 = 100
    range3 = 1000
    range4 = 10000
    range5 = 100000
    range6 = 1000000
    range7 = 5000000
    xList = list(range(range0, range1, 1)) + list(range(range1, range2, 10)) + list(range(range2, range3, 100)) + list(range(range3, range4, 1000)) + list(range(range4, range5, 10000)) + list(range(range5, range6, 100000)) + list(range(range6, range7+1, 1000000))
    names = []
    yMissLists = []

    post = time.time()
    print(post-pre)
    pre=post
       
    print("lruStack")
    stackDist = lruStack(pidAndNext, heatUp)
    post = time.time()
    print(post-pre)
    pre=post

    xList = [x for x in xList if x< max(stackDist) + 1000]

    print("Buffers to calculate: {}".format(len(xList)))

    print("Stacksize: " + str(max(stackDist)))
    lruMissList = list(map(lambda size: evalStack(stackDist, size), xList))
    minMiss = lruMissList[-1]

    lruMissList = list(map(lambda x: float(x)/elements, lruMissList))

    names.append("lru")
    yMissLists.append(lruMissList)

    post = time.time()
    print(post-pre)
    pre=post
    
    if not quick:
        print("rand")
        ranMissList = Parallel(n_jobs=8)(delayed(ran)(pidAndNext, size, heatUp=heatUp) for size in xList)
        #results = map(lambda size: ran(pidAndNext, size, heatUp=heatUp), xList))

        ranMissList = list(map(lambda x: float(x)/elements, ranMissList))
        names.append("ran")
        yMissLists.append(ranMissList)
        
        post = time.time()
        print(post-pre)
        pre=post

        print("opt")
        lastMiss = 0
        # results = (map(lambda size: belady(pidAndNext, size, heatUp=heatUp, opt=True, minMiss=minMiss, maxHit=maxHit), xList))
        optMissList = Parallel(n_jobs=8)(delayed(belady)(pidAndNext, size, heatUp=heatUp) for size in xList)

        optMissList = list(map(lambda x: float(x)/elements, optMissList))
        names.append("opt")
        yMissLists.append(optMissList)

        post = time.time()
        print(post-pre)
        pre=post

    def save_csv(xList, yMissLists, names, name):
        d = {"X": xList}
        for x in range(0, len(names)):
            d[names[x]] = yMissLists[x]
        df = pandas.DataFrame(data=d)
        df.to_csv(name+".csv", index=False)

    save_csv(xList, yMissLists, names, name)
    

def plotGraph(name, all=False):
    df = pandas.read_csv(name+".csv")
    def plotGraphInner(df, title, ylabel, file, file_data=[], miss=True):
        labels = list(df.head())
        labels.remove("X")
        for pos in range(0, len(labels)):
            misses = df[labels[pos]]
            label= labels[pos]
            if not miss:
                misses = list(map(lambda x: 1-x, misses))
            plt.plot(df["X"], misses, label=label)

        if len(file_data) == 2:
            plt.plot(file_data[0], file_data[1], label="Leanstore", linewidth=1)

        plt.xlabel("Buffer Size")
        plt.ylabel(ylabel)
        plt.ylim(0,1)
        plt.legend()
        plt.title(title)
        plt.savefig(file)
        plt.clf()

    if(all):
        print("Evaluate Files")
        (file_x, file_miss, file_hit) = createFileList(max(df["X"]))

        plotGraphInner(df, "Hitrate", "Hits", name + "_hits.pdf", file_data=[file_x, file_hit], miss=False)

        plotGraphInner(df, "Missrate", "Misses", name + "_misses.pdf", file_data=[file_x, file_miss])

    else:
        plotGraphInner(df, "Hitrate", "Hits", name + "_hits.pdf", miss=False)

        plotGraphInner(df, "Missrate", "Misses", name + "_misses.pdf")

def doOneRun(heatUp, all, data, name):
    elements= len(data)-heatUp
    dirpath = "./Elem_" + str(elements)+ "_heatUp_" + str(heatUp)
    if all:
        dirpath = "./ALL_heatUp_" + str(heatUp)
    
    try:
        os.mkdir(dirpath)
    except:
        pass

    print("Length data {}: {}".format(name, len(data)))
    try:
        print("try to load from cache")
        plotGraph(dirpath + name, all=all)
    except:
        print("Failed to load from cache")
        generateCSV(data, dirpath + name, heatUp=heatUp, all=all)
        plotGraph(dirpath + name, all=all)


def do_run():
    elementList = [10000, 100000, 1000000, -1]
    heatUpList = [0, 400, 1000, 4000, 10000, 40000]

    data = None

    if -1 in elementList:
        data_file = getSwips("./access_lists/10000011.txt", 0, all=True)
    else:
        elements = max(elementList) + max(heatUpList)
        data_file = getSwips("./access_lists/10000011.txt", elements)

    for (data, name) in [(data_file, "/Data")]:
        for elements in [-1]: # elementList:
            for heatUp in [0]: # heatUpList:
                print("-------")
                print("Elements: {}, heatup: {}".format(elements, heatUp))
                if elements == -1:
                    doOneRun(heatUp, True, data, name)
                else:
                    doOneRun(heatUp, False, data[:elements+heatUp], name)

def createFileList(UpperBound):
    def evaluateFile(file_name):
        swips = getSwips(file_name, 0, all=True)
        hot_swips = getOnlyHotSwip(swips); # In buffer
        cool_swips = getOnlyCoolSwip(swips) # In buffer, marked as evicted
        cold_swips = getOnlyColdSwip(swips) # evicted
        total = len(swips)
        hot = len(hot_swips)
        cool = len(cool_swips)
        cold = len(cold_swips)
        # print("Total: {}, Hot: {}, Cool: {}, Cold: {}".format(total, hot, cool, cold))
        # print("Missrate: {}, Hitrate: {}".format(float(cold)/total, float(hot + cool)/total))
        return (float(cold)/total, float(hot + cool)/total)

    file_name_list = list(range(21, 111, 10)) + list(range(111, 1011, 100)) + list(range(1011, 10011, 1000)) + list(range(10011, 100011, 10000)) + list(range(100011, 1000011, 100000)) + list(range(1000011, 10000011, 1000000))

    outlist = [];
    for buffer_size_name in [x for x in file_name_list if x < UpperBound]:
        try:
            outlist.append((buffer_size_name-11, evaluateFile("./access_lists/{}.txt".format(buffer_size_name))))
        except FileNotFoundError:
            print("File {} not found".format(buffer_size_name))
            pass

    (x, miss_hit) = zip(*outlist)
    (miss,hit) = zip(*miss_hit)
    return (x, miss, hit)

do_run()
# Second Try: dont evict during locking (from resolve swip to unlock)
# Third try???

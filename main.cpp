

#include <iostream>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
using namespace std;

#define OFFLINE

#ifdef OFFLINE
#define _FILE "main.cpp"
#define PRINTFUNCTION(format, ...)    fprintf(stdout, format, ##__VA_ARGS__);
#define LOG_FMT             "%-18s | "
#define LOG_INFO(message, args...)  PRINTFUNCTION(LOG_FMT message "\n" , timenow(), ## args)
#else
#define LOG_INFO(message, args...)
#endif
static inline char *timenow() {
    static char buffer[80] = {0};
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    int milli = curTime.tv_usec / 1000;
    struct tm nowTime;
    localtime_r(&curTime.tv_sec, &nowTime);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &nowTime);
    
    char currentTime[84] = {0};
    snprintf(currentTime, sizeof(currentTime), "%s:%03d", buffer, milli);
    
    sprintf(buffer,"[%s]", currentTime);
    return buffer;
}

const int maxClientNum = 35;
const int maxSiteNum = 135;
const int maxDailyStreamTypeNum = 100;
const int maxTimeNum = 8928;

int qosConstraint;
int baseCost;
double centerCost;

string dataPath, solutionPath;

int siteNum;
int clientNum;
int streamNum;
int timeNum;
int timeNum95=0;
int timeNum90=0;
int timeNum05=0;
vector<string> clientNames;
vector<string> siteNames;
unordered_map<string, int> clientNameIdMap;
unordered_map<string, int> timeNameIdMap;
unordered_map<string, int> siteNameIdMap;



int** clientDailyNeedSum;


int* clientQosDegree;
int* siteQosDegree;
int* siteIdsByQosDegree;


int** dailyStreamIds;
int* dailyStreamIdsNum;


int* siteBandwidths;
bool** qosMatrix;

int*** dailySiteTypeMaxNeed;

int** siteDailyRemainBandwidth;
int** siteDailyOccupyBandwidth;
int** siteDailyStreamBandwidth;

int* siteNeedSum;

int* streamTimeIds;
string* streamNames;
int** streamNeeds;
int** streamInitNeeds;
int* streamNeedSum;

int* streamDailyBegin;


int* top06List;
int* maxAvaiList;

double** cacheRate;


int*** ans;



void readConfig(){
    char c[4096];
    char* p = nullptr;
    qosConstraint = baseCost = 0;
    centerCost = 0;
    ifstream inFile(dataPath+"config.ini", ios::in);
    inFile.getline(c, 4096);
    // qosConstraint
    inFile.getline(c, 4096);
    p = c;
    while(*p!='=') p++;
    p++;
    while(*p>='0'&&*p<='9'){
        qosConstraint = qosConstraint * 10 + *p - '0';
        p++;
    }
    // baseCost
    inFile.getline(c, 4096);
    p = c;
    while(*p!='=') p++;
    p++;
    while(*p>='0'&&*p<='9'){
        baseCost = baseCost * 10 + *p - '0';
        p++;
    }
    // centerCost
    inFile.getline(c, 4096);
    p = c;
    while(*p!='=') p++;
    p++;
    string centerCostStr = "";
    while(*p!='\r'&&*p!='\n'&&*p!='\0'){
        centerCostStr += *p;
        p++;
    }
    centerCost = atof(centerCostStr.c_str());
    // log
    LOG_INFO("config_qosConstaint: %d", qosConstraint)
    LOG_INFO("config_baseCost: %d", baseCost)
    LOG_INFO("config_centerCost: %lf", centerCost)
}
void readDemand(){
    int fd = open((dataPath+"demand.csv").c_str(), O_RDONLY);
    int dataSize = (int)lseek(fd, 0, SEEK_END);
    char* dataChar = (char*)mmap(NULL, dataSize, PROT_READ, MAP_PRIVATE, fd, 0);
    char* pt= dataChar;
    char* ed = dataChar+dataSize;
    
    // client count
    pt = dataChar;
    int clientCnt = 0;
    while(*pt!='\r'){
        if(*pt==',')
            clientCnt++;
        pt++;
    }
    clientCnt -= 1;
    clientNum = clientCnt;
    
    // stream count
    pt = dataChar;
    int streamCnt = 0;
    while(pt<ed){
        if(*pt=='\r')
            streamCnt++;
        pt++;
    }
    streamNum = streamCnt - 1;
    streamNames = new string[streamNum];
    streamTimeIds = new int[streamNum]();
    
    

    
    // headLine
    pt= dataChar;
    while(*pt!=',') pt++;
    pt++;
    while(*pt!=',') pt++;
    pt++;
    int clientId = 0;
    while(true){
        string clientName = "";
        while(*pt!=','&&*pt!='\r'&&*pt!='\n'){
            clientName += *pt;
            pt++;
        }
        clientNames.push_back(clientName);
        clientNameIdMap[clientName] = clientId;
        clientId++;
        if(*pt=='\r'||*pt=='\n')
            break;
        pt++;
    }
    while(*pt=='\r'||*pt=='\n')
        pt++;
    
    
    streamNeeds = new int*[streamNum];
    for(int i=0;i<streamNum;i++){
        streamNeeds[i] = new int[clientNum]();
    }
    streamInitNeeds = new int*[streamNum];
    for(int i=0;i<streamNum;i++){
        streamInitNeeds[i] = new int[clientNum]();
    }
    
    
    
    streamNeedSum = new int[streamNum]();
    
    
    // demands
    int streamTotalCnt = 0;
    int timeCnt = 0;
    while(pt<ed){
        string timeName = "";
        string streamTypeName = "";
        while(*pt!=','){
            timeName += *pt;
            pt++;
        }
        pt++;
        while(*pt!=','){
            streamTypeName += *pt;
            pt++;
        }
        pt++;
        
        int streamId = streamTotalCnt;

        auto timeIter = timeNameIdMap.find(timeName);
        if(timeIter==timeNameIdMap.end()){
            streamTimeIds[streamId] = timeCnt;
            timeNameIdMap[timeName] = timeCnt;
            timeCnt++;
        }
        else{
            streamTimeIds[streamId] = timeIter->second;
        }
        
        
        streamNames[streamId] = streamTypeName;
    
        
        for(int clientId=0;clientId<clientNum;clientId++){
            int need = 0;
            while(*pt>='0'&&*pt<='9'){
                need = need * 10 + *pt - '0';
                pt++;
            }
            streamNeeds[streamId][clientId] = need;
            streamInitNeeds[streamId][clientId] = need;
            streamNeedSum[streamId] += need;
            pt++;
        }
        while(pt<ed && (*pt=='\r'||*pt=='\n'))
            pt++;
        streamTotalCnt++;
    }
    
    timeNum = timeCnt;
    timeNum95 = (int)ceil(timeNum * 0.95);
    timeNum90 = (int)ceil(timeNum * 0.90);
    timeNum05 = timeNum - timeNum95;
    
    
    
    
    
    
    
    
    
    LOG_INFO("clientNum: %d", clientNum)
    LOG_INFO("timeNum: %d", timeNum)
    LOG_INFO("streamTotalNum: %d", streamNum)
}
void readSiteBandwidth(){
    
    int fd = open((dataPath+"site_bandwidth.csv").c_str(), O_RDONLY);
    int dataSize = (int)lseek(fd, 0, SEEK_END);
    char* dataChar = (char*)mmap(NULL, dataSize, PROT_READ, MAP_PRIVATE, fd, 0);
    char* pt= dataChar;
    char* ed = dataChar+dataSize;
    
    // site count
    pt= dataChar;
    int siteCnt = -1;
    while(pt<ed){
        if(*pt=='\n')
            siteCnt++;
        pt++;
    }
    siteNum = siteCnt;
    
    
    siteBandwidths = new int[siteNum]();
    
    
    // bandwidth
    pt= dataChar;
    while(*pt!='\n') pt++;
    pt++;
    siteCnt = 0;
    while(pt<ed){
        string siteName = "";
        int siteBandwidth = 0;
        while(*pt!=','){
            siteName += *pt;
            pt++;
        }
        pt++;
        while(*pt>='0'&&*pt<='9'){
            siteBandwidth = siteBandwidth * 10 + *pt - '0';
            pt++;
        }
        while(pt<ed && (*pt=='\r'||*pt=='\n'))
            pt++;
        siteBandwidth = max(0,siteBandwidth-1);
        int siteId = siteCnt;
        siteNameIdMap[siteName] = siteId;
        siteNames.push_back(siteName);
        siteBandwidths[siteId] = siteBandwidth;
        siteCnt++;
    }
    LOG_INFO("siteNum: %d", siteNum)
}

void readQos(){
    int fd = open((dataPath+"qos.csv").c_str(), O_RDONLY);
    int dataSize = (int)lseek(fd, 0, SEEK_END);
    char* dataChar = (char*)mmap(NULL, dataSize, PROT_READ, MAP_PRIVATE, fd, 0);
    char* pt= dataChar;
    char* ed = dataChar+dataSize;
    
    qosMatrix = new bool*[siteNum];
    for(int i=0;i<siteNum;i++)
        qosMatrix[i] = new bool[clientNum]();
    
    pt= dataChar;
    while(*pt!=',') pt++;
    pt++;
    vector<int> clientIdVec;
    for(int i=0;i<clientNum;i++){
        string clientName = "";
        while(*pt!=','&&*pt!='\r'){
            clientName += *pt;
            pt++;
        }
        pt++;
        int clientId = clientNameIdMap[clientName];
        clientIdVec.push_back(clientId);
    }
    while(pt<ed && (*pt=='\r'||*pt=='\n'))
        pt++;
    int validQosCnt = 0;
    while (pt<ed) {
        string siteName = "";
        while(*pt!=','){
            siteName += *pt;
            pt++;
        }
        pt++;
        int siteId = siteNameIdMap[siteName];
        for(int i=0;i<clientNum;i++){
            int qosNum = 0;
            while(*pt!=','&&*pt!='\r'&&pt<ed){
                qosNum = qosNum * 10 + *pt - '0';
                pt++;
            }
            pt++;
            int clientId = clientIdVec[i];
            qosMatrix[siteId][clientId] = (qosNum<qosConstraint);
            if(qosNum<qosConstraint){
                validQosCnt++;
            }
        }
        while(pt<ed && (*pt=='\r'||*pt=='\n'))
            pt++;
    }
    LOG_INFO("qosRate: %.2lf", validQosCnt*1.0/(clientNum*siteNum))
    
}

void readData(){
#ifdef OFFLINE
    dataPath = "/Users/wangzhoujie/Desktop/huawei/huawei/data/";
    solutionPath = "/Users/wangzhoujie/Desktop/huawei/huawei/output/solution.txt";
#else
    dataPath = "/data/";
    solutionPath = "/output/solution.txt";
#endif
    readConfig();
    readDemand();
    readSiteBandwidth();
    readQos();
    
}

void prepare(){
    siteQosDegree = new int[siteNum]();
    vector<pair<int,int>> siteQosIdPairVec;
    for(int siteId=0;siteId<siteNum;siteId++){
        int qosDegree = 0;
        for(int clientId=0;clientId<clientNum;clientId++){
            if(qosMatrix[siteId][clientId])
                qosDegree++;
        }
        if(siteBandwidths[siteId]==0)
            qosDegree = 0;
        siteQosDegree[siteId] = qosDegree;
        siteQosIdPairVec.push_back(make_pair(-qosDegree, siteId));
    }
    sort(siteQosIdPairVec.begin(),siteQosIdPairVec.end());
    siteIdsByQosDegree = new int[siteNum]();
    
    for(int i=0;i<siteQosIdPairVec.size();i++){
        siteIdsByQosDegree[i] = siteQosIdPairVec[i].second;
    }
    
    
    clientQosDegree = new int[clientNum]();
    for(int clientId=0;clientId<clientNum;clientId++){
        int qosDegree = 0;
        for(int siteId=0;siteId<siteNum;siteId++){
            if(qosMatrix[siteId][clientId])
                qosDegree++;
        }
        clientQosDegree[clientId] = qosDegree;
    }
    
    
    
    siteDailyRemainBandwidth = new int*[timeNum];
    for(int t=0;t<timeNum;t++){
        siteDailyRemainBandwidth[t] = new int[siteNum]();
        for(int i=0;i<siteNum;i++){
            siteDailyRemainBandwidth[t][i] = siteBandwidths[i];
        }
    }
    
    siteDailyOccupyBandwidth = new int*[timeNum];
    for(int t=0;t<timeNum;t++){
        siteDailyOccupyBandwidth[t] = new int[siteNum]();
    }
    
    siteDailyStreamBandwidth = new int*[timeNum];
    for(int t=0;t<timeNum;t++){
        siteDailyStreamBandwidth[t] = new int[siteNum]();
    }
    
    clientDailyNeedSum = new int*[timeNum];
    for(int t=0;t<timeNum;t++){
        clientDailyNeedSum[t] = new int[clientNum]();
    }
    
    for(int streamId=0;streamId<streamNum;streamId++){
        int timeId = streamTimeIds[streamId];
        for(int clientId=0;clientId<clientNum;clientId++){
            int tmp = clientDailyNeedSum[timeId][clientId] ;
            clientDailyNeedSum[timeId][clientId] += streamNeeds[streamId][clientId];
            
        }
    }
    
//    for(int clientId=0;clientId<clientNum;clientId++){
//        cout<<clientDailyNeedSum[0][clientId]<<' ';
//    }
//    cout<<endl;
//
//    for(int clientId=0;clientId<clientNum;clientId++){
//        cout<<clientDailyNeedSum[1][clientId]<<' ';
//    }
//    cout<<endl;
    
    
    
    streamDailyBegin = new int[timeNum+1]();
    int curStreamId = 0;
    for(int timeId=0;timeId<timeNum;timeId++){
        streamDailyBegin[timeId] = curStreamId;
        while(curStreamId<streamNum&&streamTimeIds[curStreamId]==timeId){
            curStreamId++;
        }
    }
    streamDailyBegin[timeNum] = streamNum;
    
    
    
    ans = new int**[timeNum];
    for(int t=0;t<timeNum;t++){
        ans[t] = new int*[clientNum];
        for(int i=0;i<clientNum;i++){
            ans[t][i] = new int[maxDailyStreamTypeNum]();
            for(int j=0;j<maxDailyStreamTypeNum;j++)
                ans[t][i][j] = -1;
        }
    }
    
    
    dailySiteTypeMaxNeed = new int**[timeNum];
    for(int t=0;t<timeNum;t++){
        dailySiteTypeMaxNeed[t] = new int*[siteNum];
        for(int s=0;s<siteNum;s++)
            dailySiteTypeMaxNeed[t][s] = new int[maxDailyStreamTypeNum]();
    }
    
    top06List = new int[siteNum]();
    maxAvaiList = new int[siteNum]();
    
    
    siteNeedSum = new int[siteNum]();
    
    cacheRate = new double*[timeNum];
    for(int t=0;t<timeNum;t++){
        cacheRate[t] = new double[siteNum]();
        for(int s=0;s<siteNum;s++)
            cacheRate[t][s] = 0.05;
    }
    
}


int findBetterSite(bool* visitedSiteSet){
    int betterSite = -1;
    double betterScore = 0;
    for(int i=0;i<siteNum;i++){
        int siteId = siteIdsByQosDegree[i];
        if(siteQosDegree[siteId]==0) continue;
        if(visitedSiteSet[siteId]) continue;
        double score = 0;
        for(int timeId=0;timeId<timeNum;timeId++){
            for(int clientId=0;clientId<clientNum;clientId++){
                if(qosMatrix[siteId][clientId])
                    score += clientDailyNeedSum[timeId][clientId];
            }
        }
        if(score>betterScore){
            betterScore = score;
            betterSite = siteId;
        }
    }
    if(betterSite!=-1)
        visitedSiteSet[betterSite] = true;
    return betterSite;
}

bool judgeMaxAvai(int maxAvai, int timeId, int siteId){
    int curTimeId = timeId;
    int curCache = maxAvai;
    while(true){
        if(curTimeId>=timeNum||curCache==0) break;
        if(siteDailyOccupyBandwidth[curTimeId][siteId]+curCache>siteBandwidths[siteId])
            return false;
        curCache = (int)floor(curCache*cacheRate[curTimeId][siteId]);
        curTimeId++;
    }
    return true;
}

int findMaxAvai(int currentAvai, int timeId, int siteId, int interval){
    int maxAvai = 0;
    while(true){
        if(currentAvai<=0) break;
        if(judgeMaxAvai(currentAvai,timeId,siteId)){
            maxAvai = currentAvai;
            break;
        }
        currentAvai -= interval;
    }
    return maxAvai;
}

double scoreTimeForSite(int timeId, int siteId){
    int maxAvai = siteDailyRemainBandwidth[timeId][siteId];
    maxAvai = findMaxAvai(maxAvai, timeId, siteId, 40);
    double score = 0;
    for(int clientId=0;clientId<clientNum;clientId++){
        if(!qosMatrix[siteId][clientId]) continue;
        score += clientDailyNeedSum[timeId][clientId]/pow(clientQosDegree[clientId], 0.8);
    }
    score = min((double)maxAvai, score);
    return score;
}

void recordAnswer(int timeId, int clientId, int siteId, int streamId){
    int need = streamNeeds[streamId][clientId];
    
    
    if(need==0){
        cout<<"zero allocation"<<endl;
        exit(0);
    }
    int& record = ans[timeId][clientId][streamId-streamDailyBegin[timeId]];
    if(record!=-1){
        cout<<"repeat allocation"<<endl;
        exit(0);
    }
    record = siteId;
}


void freshCacheForSite(int siteId, int timeId, int cache){
    for(int newTimeId=timeId;newTimeId<timeNum-1;newTimeId++){
        cache = (int)floor(cache*cacheRate[newTimeId][siteId]);
        if(cache==0)
            break;
        siteDailyRemainBandwidth[newTimeId+1][siteId] -= cache;
        siteDailyOccupyBandwidth[newTimeId+1][siteId] += cache;
        
    }
    for(int newTimeId=timeId;newTimeId>0;newTimeId--){
        if(cacheRate[newTimeId-1][siteId]>0.04){
            if(siteDailyRemainBandwidth[newTimeId][siteId]*20+19 < siteDailyRemainBandwidth[newTimeId-1][siteId])
                siteDailyRemainBandwidth[newTimeId-1][siteId] = siteDailyRemainBandwidth[newTimeId][siteId]*20+19;
            else
                break;
        }
        else{
            if(siteDailyRemainBandwidth[newTimeId][siteId]*100+99 < siteDailyRemainBandwidth[newTimeId-1][siteId])
                siteDailyRemainBandwidth[newTimeId-1][siteId] = siteDailyRemainBandwidth[newTimeId][siteId]*100+99;
            else
                break;
        }
    }
}


pair<int,int> dailyStreamPair[maxDailyStreamTypeNum];
pair<int,int> clientPair[maxClientNum];


int allocTime05ForSite(int timeId, int siteId){
    //cout<<siteId<<'>'<<timeId<<' ';
    int allocatedNeed = 0;
    int maxAvai = siteDailyRemainBandwidth[timeId][siteId];
    maxAvai = findMaxAvai(maxAvai, timeId, siteId, 5);
    maxAvai = max(maxAvai-3000, 0);
    if(maxAvai<=0)
        return 0;
    int currentStreamNum = streamDailyBegin[timeId+1] - streamDailyBegin[timeId];
    for(int streamType=0;streamType<currentStreamNum;streamType++){
        dailyStreamPair[streamType].second = streamType;
        int needSum = 0;
        int needMax = 0;
        int streamId = streamType + streamDailyBegin[timeId];
        for(int clientId=0;clientId<clientNum;clientId++){
            if(!qosMatrix[siteId][clientId]) continue;
            needSum += streamNeeds[streamId][clientId];
            needMax = max(needMax, streamNeeds[streamId][clientId]);
        }
        dailyStreamPair[streamType].first = needSum;//(needSum-needMax+needSum/2)*100/(needSum+1);
    }
    sort(dailyStreamPair,dailyStreamPair+currentStreamNum,
         [](pair<int,int>& p1, pair<int,int>& p2){
             return p1.first > p2.first;
    });
    
    for(int i=0;i<currentStreamNum;i++){
        int streamNeedSum = dailyStreamPair[i].first;
        int streamType = dailyStreamPair[i].second;
        if(streamNeedSum==0) break;
        int streamId = streamType + streamDailyBegin[timeId];
        if(streamNeedSum + allocatedNeed > maxAvai) continue;
        int clientPairCnt = 0;
        for(int clientId=0;clientId<clientNum;clientId++){
            if(!qosMatrix[siteId][clientId]) continue;
            int need = streamNeeds[streamId][clientId];
            if(need>0){
                clientPair[clientPairCnt].first = need;
                clientPair[clientPairCnt].second = clientId;
                clientPairCnt++;
            }
        }
        sort(clientPair,clientPair+clientPairCnt,
             [](pair<int,int>& p1, pair<int,int>& p2){
                 return p1.first > p2.first;
        });
        for(int j=0;j<clientPairCnt;j++){
            int need = clientPair[j].first;
            int clientId = clientPair[j].second;
            if (need + allocatedNeed <= maxAvai) {
                recordAnswer(timeId, clientId, siteId, streamId);
                clientDailyNeedSum[timeId][clientId] -= need;
                streamNeeds[streamId][clientId] = 0;
                siteDailyRemainBandwidth[timeId][siteId] -= need;
                siteDailyOccupyBandwidth[timeId][siteId] += need;
                dailySiteTypeMaxNeed[timeId][siteId][streamType] = max(need, dailySiteTypeMaxNeed[timeId][siteId][streamType]);

                allocatedNeed += need;
            }
        }
    }
    
    // resort
    
    
//    for(int streamType=0;streamType<currentStreamNum;streamType++){
//        dailyStreamPair[streamType].first = 0;
//        dailyStreamPair[streamType].second = streamType;
//        int streamId = streamType + streamDailyBegin[timeId];
//        for(int clientId=0;clientId<clientNum;clientId++){
//            if(!qosMatrix[siteId][clientId]) continue;
//            dailyStreamPair[streamType].first += streamNeeds[streamId][clientId];
//        }
//    }
//    sort(dailyStreamPair,dailyStreamPair+currentStreamNum,
//         [](pair<int,int>& p1, pair<int,int>& p2){
//             return p1.first > p2.first;
//         });
    
    for(int i=0;i<currentStreamNum;i++){
        int streamNeedSum = dailyStreamPair[i].first;
        int streamType = dailyStreamPair[i].second;
        if(streamNeedSum==0) break;
        int streamId = streamType + streamDailyBegin[timeId];
        int clientPairCnt = 0;
        for(int clientId=0;clientId<clientNum;clientId++){
            if(!qosMatrix[siteId][clientId]) continue;
            int need = streamNeeds[streamId][clientId];
            if(need>0){
                clientPair[clientPairCnt].first = need;
                clientPair[clientPairCnt].second = clientId;
                clientPairCnt++;
            }
        }
        sort(clientPair,clientPair+clientPairCnt,
             [](pair<int,int>& p1, pair<int,int>& p2){
                 return p1.first > p2.first;
             });
        for(int j=0;j<clientPairCnt;j++){
            int need = clientPair[j].first;
            int clientId = clientPair[j].second;
            if (need + allocatedNeed <= maxAvai) {
                recordAnswer(timeId, clientId, siteId, streamId);
                clientDailyNeedSum[timeId][clientId] -= need;
                streamNeeds[streamId][clientId] = 0;
                siteDailyRemainBandwidth[timeId][siteId] -= need;
                siteDailyOccupyBandwidth[timeId][siteId] += need;
                dailySiteTypeMaxNeed[timeId][siteId][streamType] = max(need, dailySiteTypeMaxNeed[timeId][siteId][streamType]);
                
                allocatedNeed += need;
            }
        }
    }
    
    if(allocatedNeed>0){
        freshCacheForSite(siteId, timeId, allocatedNeed);
    }
    //cout<<allocatedNeed<<endl;

    return allocatedNeed;

}

bool** visitedTop5Days;



int allocTime95ForSite(int timeId, int siteId, int siteTop06){
    //cout<<timeId<<' '<<siteId<<' '<<siteTop06<<endl;
    int allocatedNeed = 0;
    int maxAvai = min(siteDailyRemainBandwidth[timeId][siteId], siteTop06 - siteDailyOccupyBandwidth[timeId][siteId]);
    maxAvai = findMaxAvai(maxAvai, timeId, siteId, 5);
    if(maxAvai<=0)
        return 0;
    
    int currentStreamNum = streamDailyBegin[timeId+1] - streamDailyBegin[timeId];
    for(int streamType=0;streamType<currentStreamNum;streamType++){
        dailyStreamPair[streamType].second = streamType;
        int needSum = 0;
        int needMax = 0;
        int streamId = streamType + streamDailyBegin[timeId];
        for(int clientId=0;clientId<clientNum;clientId++){
            if(!qosMatrix[siteId][clientId]) continue;
            needSum += streamNeeds[streamId][clientId];
            needMax = max(needMax, streamNeeds[streamId][clientId]);
        }
        dailyStreamPair[streamType].first = needSum;//(needSum-needMax+needSum/2)*100/(needSum+1);
    }
    sort(dailyStreamPair,dailyStreamPair+currentStreamNum,
         [](pair<int,int>& p1, pair<int,int>& p2){
             return p1.first > p2.first;
    });
    
    for(int i=0;i<currentStreamNum;i++){
        int streamNeedSum = dailyStreamPair[i].first;
        int streamType = dailyStreamPair[i].second;
        if(streamNeedSum==0) break;
        int streamId = streamType + streamDailyBegin[timeId];
        if(streamNeedSum + siteDailyOccupyBandwidth[timeId][siteId] > maxAvai) continue;
        int clientPairCnt = 0;
        for(int clientId=0;clientId<clientNum;clientId++){
            if(qosMatrix[siteId][clientId]){
                int need = streamNeeds[streamId][clientId];
                if(need>0){
                    clientPair[clientPairCnt].first = need;
                    clientPair[clientPairCnt].second = clientId;
                    clientPairCnt++;
                }
            }
        }
        sort(clientPair,clientPair+clientPairCnt,
             [](pair<int,int>& p1, pair<int,int>& p2){
                 return p1.first > p2.first;
             });
        for(int j=0;j<clientPairCnt;j++){
            int need = clientPair[j].first;
            int clientId = clientPair[j].second;
            if (need + siteDailyOccupyBandwidth[timeId][siteId] <= maxAvai) {
                recordAnswer(timeId, clientId, siteId, streamId);
                clientDailyNeedSum[timeId][clientId] -= need;
                streamNeeds[streamId][clientId] = 0;
                
                
                siteDailyRemainBandwidth[timeId][siteId] -= need;
                siteDailyOccupyBandwidth[timeId][siteId] += need;
                
                dailySiteTypeMaxNeed[timeId][siteId][streamType] = max(need, dailySiteTypeMaxNeed[timeId][siteId][streamType]);
                allocatedNeed += need;
                
                
            }
        }
    }
    if(allocatedNeed>0){
        freshCacheForSite(siteId, timeId, allocatedNeed);
    }
    return allocatedNeed;
    
}
bool** blockedTop5Days;

int findBetterTime(int siteId){
    double betterScore = 0;
    int betterTimeId = -1;
    for(int timeId=0;timeId<timeNum;timeId++){
        if(visitedTop5Days[siteId][timeId]) continue;
        if(blockedTop5Days[siteId][timeId]) continue;
        double score = scoreTimeForSite(timeId, siteId);
        if(score>betterScore){
            betterScore = score;
            betterTimeId = timeId;
        }
    }
    return betterTimeId;
}

double scoreTimeForSite0(int timeId, int siteId){
    double score = 0;
    for(int clientId=0;clientId<clientNum;clientId++){
        if(!qosMatrix[siteId][clientId]) continue;
        score += clientDailyNeedSum[timeId][clientId]/pow(clientQosDegree[clientId], 0.7);
    }
    return score;
}


void phase0_run(){
    LOG_INFO("phase0 begin")

    
    pair<double,int>** scoreMatric = new pair<double,int>*[timeNum];
    for(int t=0;t<timeNum;t++)
        scoreMatric[t] = new pair<double,int>[siteNum];
    
    for(int t=0;t<timeNum;t++){
        for(int s=0;s<siteNum;s++){
            scoreMatric[t][s] = make_pair(scoreTimeForSite0(t,s),s);
        }
        sort(scoreMatric[t],scoreMatric[t]+siteNum,[](pair<double,int>& p1, pair<double,int>& p2){
            return p1.first > p2.first;
        });
        for(int s=0;s<20;s++){
            cacheRate[t][scoreMatric[t][s].second] = 0.01;
        }
    }

    
    LOG_INFO("phase0 end")
    
}

void phase1_run(){
    LOG_INFO("phase1 begin")
    bool* visitedSite = new bool[siteNum]();
    visitedTop5Days = new bool*[siteNum];
    for(int s=0;s<siteNum;s++){
        visitedTop5Days[s] = new bool[timeNum]();
    }
    blockedTop5Days = new bool*[siteNum];
    for(int s=0;s<siteNum;s++){
        blockedTop5Days[s] = new bool[timeNum]();
    }
    pair<int,int>* scoreTimePairVec = new pair<int,int>[timeNum];
    
    for(int round=0;round<siteNum;round++){
        int siteId = findBetterSite(visitedSite);
        if(siteId==-1) continue;
        
        for(int timeId=0;timeId<timeNum;timeId++){
            scoreTimePairVec[timeId].first = scoreTimeForSite(timeId, siteId);
            scoreTimePairVec[timeId].second = timeId;
        }
        sort(scoreTimePairVec,scoreTimePairVec+timeNum,
             [](pair<int,int>& p1, pair<int,int>& p2){
                 return p1.first > p2.first;
        });
        for(int i=0;i<timeNum05;i++){
            int timeId = scoreTimePairVec[i].second;
            int allocatedNeed = allocTime05ForSite(timeId, siteId);
            if(allocatedNeed>0)
                visitedTop5Days[siteId][timeId] = true;
            else
                blockedTop5Days[siteId][timeId] = true;
        }
        

        for(int clientId=0;clientId<clientNum;clientId++){
            if(qosMatrix[siteId][clientId])
                clientQosDegree[clientId]--;
        }
        
        
        int siteTop06 = 0;
        for(int timeId=0;timeId<timeNum;timeId++){
            if(visitedTop5Days[siteId][timeId]) continue;
            siteTop06 = max(siteTop06, siteDailyOccupyBandwidth[timeId][siteId]);
        }
        if(siteTop06<=baseCost)
            siteTop06 = baseCost;
        
        
//        if(siteQosDegree[siteId]>14){
//            siteTop06 = max(siteTop06,int(siteBandwidths[siteId]*0.05));
//        }
        

        for(int timeId=0;timeId<timeNum;timeId++){
            if(visitedTop5Days[siteId][timeId]) continue;
            int allocatedNeed = allocTime95ForSite(timeId, siteId, siteTop06);

        }
        
    }
//    int maxFlow = 0;
//    for(int i=0;i<streamNum;i++){
//        for(int j=0;j<clientNum;j++){
//            maxFlow = max(maxFlow, streamNeeds[i][j]);
//        }
//    }
//    cout<<maxFlow<<endl;
//    int usedSite = 0;
//    for(int i=0;i<siteNum;i++){
//        for(int t=0;t<timeNum;t++){
//            if(siteDailyOccupyBandwidth[t][i]>0){
//                usedSite ++;
//                break;
//            }
//        }
//    }
//    cout<<usedSite<<endl;
    
    
    LOG_INFO("phase1 end")
    
}



int findFreeSiteForClient(int timeId, int streamId, int clientId){
    
    int curNeed = streamNeeds[streamId][clientId];
    int streamType = streamId - streamDailyBegin[timeId];
    
    for(int siteId=0;siteId<siteNum;siteId++)
        siteNeedSum[siteId] = 0;
    
    for(int siteId=0;siteId<siteNum;siteId++){
        if(!qosMatrix[siteId][clientId]) continue;
        if(visitedTop5Days[siteId][timeId]) continue;
        int maxAvai = min(maxAvaiList[siteId], top06List[siteId]-siteDailyOccupyBandwidth[timeId][siteId]);
        if(maxAvai<=0) continue;
        int needSum = 0;
        for(int clientId=0;clientId<clientNum;clientId++){
            if(!qosMatrix[siteId][clientId]) continue;
            needSum += streamNeeds[streamId][clientId];
        }
        if(needSum <= maxAvai){
            siteNeedSum[siteId] = needSum;
        }
    }
    
    int bestSiteId = -1;
    int bestCost = 0x3f3f3f3f;
    
    for(int s=0;s<siteNum;s++){
        int siteId = siteIdsByQosDegree[s];
        if(!qosMatrix[siteId][clientId]) continue;
        if(visitedTop5Days[siteId][timeId]) continue;
        if(siteNeedSum[siteId]==0) continue;
        int cost = max(0, curNeed - dailySiteTypeMaxNeed[timeId][siteId][streamType]);
        if(cost<bestCost){
            bestCost = cost;
            bestSiteId = siteId;
        }
        else if(cost==bestCost&&siteNeedSum[siteId]>siteNeedSum[bestSiteId]){
            bestSiteId = siteId;
        }
    }
    
    if(bestSiteId!=-1) return bestSiteId;
    
    
    
    for(int s=0;s<siteNum;s++){
        int siteId = siteIdsByQosDegree[s];
        if(!qosMatrix[siteId][clientId]) continue;
        if(visitedTop5Days[siteId][timeId]) continue;
        int maxAvai = min(maxAvaiList[siteId], top06List[siteId]-siteDailyOccupyBandwidth[timeId][siteId]);
        if(maxAvai<curNeed) continue;
        int cost = max(0, curNeed - dailySiteTypeMaxNeed[timeId][siteId][streamType]);
        if(cost<bestCost){
            bestCost = cost;
            bestSiteId = siteId;
        }
    }
    if(bestSiteId!=-1) return bestSiteId;
    
    return -1;
}

double getSiteCost(int siteId, int siteTop06){
    //if(siteTop06==0) return 0;
    if(siteTop06<baseCost) return baseCost;
    return pow(siteTop06-baseCost,2)/siteBandwidths[siteId]+siteTop06;
}

double scoreSiteForTimeStreamClient(int siteId, int timeId, int streamId, int clientId){
    int need = streamNeeds[streamId][clientId];
    
    double futureCost = getSiteCost(siteId, siteDailyOccupyBandwidth[timeId][siteId] + need)
            - getSiteCost(siteId, top06List[siteId]);
    
    if(futureCost<0)
        futureCost = 0;
    
    double futureGain = 0;
    for(int t=0;t<timeNum;t++){
        for(int clientId=0;clientId<clientNum;clientId++){
            if(!qosMatrix[siteId][clientId]) continue;
            futureGain += clientDailyNeedSum[timeId][clientId];
        }
    }
    return futureGain/(futureCost+1e-6);
}


int findGoodSiteForClient(int timeId, int streamId, int clientId){
    double bestScore = 0;
    int bestSiteId = -1;
    int need = streamNeeds[streamId][clientId];
    for(int siteId=0;siteId<siteNum;siteId++){
        if(!qosMatrix[siteId][clientId]) continue;
        if(visitedTop5Days[siteId][timeId]) continue;
        if(siteDailyRemainBandwidth[timeId][siteId]<need) continue;
        double score = scoreSiteForTimeStreamClient(siteId, timeId, streamId, clientId);
        if(score>bestScore){
            bestScore = score;
            bestSiteId = siteId;
        }
    }
    return bestSiteId;
}

void allocTimeStreamClientForSite(int timeId, int streamId, int clientId, int siteId){
    int streamType = streamId - streamDailyBegin[timeId];
    int need = streamNeeds[streamId][clientId];
    
    if(siteDailyOccupyBandwidth[timeId][siteId]>siteBandwidths[siteId]){
        cout<<siteDailyOccupyBandwidth[timeId][siteId]<<' '<<siteBandwidths[siteId]<<' '<<maxAvaiList[siteId]<<endl;
        cout<<"overflow allocation"<<endl;
        exit(0);
    }
    
    recordAnswer(timeId, clientId, siteId, streamId);
    clientDailyNeedSum[timeId][clientId] -= need;
    streamNeeds[streamId][clientId] = 0;
    siteDailyRemainBandwidth[timeId][siteId] -= need;
    siteDailyOccupyBandwidth[timeId][siteId] += need;
    siteDailyStreamBandwidth[timeId][siteId] += need;
    dailySiteTypeMaxNeed[timeId][siteId][streamType] = max(need, dailySiteTypeMaxNeed[timeId][siteId][streamType]);
}





pair<int,int> timePair[maxTimeNum];

void phase2_run(){
    LOG_INFO("phase2 begin")
    
    int* banwidthTemp = new int[timeNum];
    for(int siteId=0;siteId<siteNum;siteId++){
        for(int timeId=0;timeId<timeNum;timeId++){
            banwidthTemp[timeId] = siteDailyOccupyBandwidth[timeId][siteId];
        }
        sort(banwidthTemp,banwidthTemp+timeNum);
        top06List[siteId] = banwidthTemp[timeNum95-1];
        if(banwidthTemp[timeNum-1]>0&&top06List[siteId]<baseCost)
            top06List[siteId] = baseCost;
    }
    
    
    
    
//    for(int timeId=0;timeId<timeNum;timeId++){
//        for(int siteId=0;siteId<siteNum;siteId++){
//            int top06 = (int)floor(siteDailyOccupyBandwidth[timeId][siteId]*0.05);
//            if(top06List[siteId]<top06)
//                top06List[siteId] = top06;
//        }
//    }
//    for(int siteId=0;siteId<siteNum;siteId++){
//        if(top06List[siteId]>0&&top06List[siteId]<baseCost)
//            top06List[siteId] = baseCost;
//    }

    // disable
//    for(int timeId=0;timeId<timeNum;timeId++){
//        timePair[timeId].first = 0;
//        for(int clientId=0;clientId<clientNum;clientId++){
//            timePair[timeId].first += clientDailyNeedSum[timeId][clientId];
//        }
//        timePair[timeId].second = timeId;
//    }
//
//    sort(timePair,timePair+timeNum,
//         [](pair<int,int>& p1, pair<int,int>& p2){
//             return p1.first > p2.first;
//    });
    
    
    for(int t=0;t<timeNum;t++){
        int timeId = t;//timePair[t].second;
        for(int siteId=0;siteId<siteNum;siteId++){
            if(siteQosDegree[siteId]==0) continue;
            int maxAvai = siteDailyRemainBandwidth[timeId][siteId];
            maxAvai = findMaxAvai(maxAvai, timeId, siteId, 5);
            maxAvaiList[siteId] = maxAvai;
        }
        for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
            int streamType = streamId - streamDailyBegin[timeId];
            dailyStreamPair[streamType].first = 0;
            dailyStreamPair[streamType].second = streamType;
            for(int clientId=0;clientId<clientNum;clientId++){
                dailyStreamPair[streamType].first += streamNeeds[streamId][clientId];
            }
        }
        sort(dailyStreamPair,dailyStreamPair+streamDailyBegin[timeId+1]-streamDailyBegin[timeId],
             [](pair<int,int>& p1, pair<int,int>& p2){
                 return p1.first > p2.first;
        });
        for(int i=0;i<streamDailyBegin[timeId+1]-streamDailyBegin[timeId];i++){
            if(dailyStreamPair[i].first==0) break;
            int streamType = dailyStreamPair[i].second;
            int streamId = streamType+streamDailyBegin[timeId];
            int clientPairCnt = 0;
            for(int clientId=0;clientId<clientNum;clientId++){
                int need = streamNeeds[streamId][clientId];
                if(need>0){
                    clientPair[clientPairCnt].first = need;
                    clientPair[clientPairCnt].second = clientId;
                    clientPairCnt++;
                }
            }
            sort(clientPair,clientPair+clientPairCnt,
                 [](pair<int,int>& p1, pair<int,int>& p2){
                     return p1.first > p2.first;
            });
            for(int j=0;j<clientPairCnt;j++){
                int need = clientPair[j].first;
                int clientId = clientPair[j].second;
                int freeSiteId = findFreeSiteForClient(timeId, streamId, clientId);
                if(freeSiteId!=-1){
                    allocTimeStreamClientForSite(timeId, streamId, clientId, freeSiteId);
                    maxAvaiList[freeSiteId] -= need;
                }
                else{
                    int goodSiteId = findGoodSiteForClient(timeId, streamId, clientId);
                    if(goodSiteId!=-1){
                        allocTimeStreamClientForSite(timeId, streamId, clientId, goodSiteId);
                        maxAvaiList[goodSiteId] -= need;
                        top06List[goodSiteId] = max(top06List[goodSiteId],siteDailyOccupyBandwidth[timeId][goodSiteId] + need);
                    }
                    else{
                        cout<<"fail allocation"<<endl;
                        exit(0);
                    }
                }
                
            }
        }
        for(int siteId=0;siteId<siteNum;siteId++){
            freshCacheForSite(siteId, timeId, siteDailyStreamBandwidth[timeId][siteId]);
        }
    }
    LOG_INFO("phase2 end")
    
}

//int** dailyBandwidthFlow;
//int** dailyBandwidthCache;
//
//
//int** dailyNeeds; // client type
//bool*** allocation; // site type client
//int** siteMaxTypeNeed; // site tpye
//
//int findDstSite(int gain, int clientNeedSum, int srcSiteId, int timeId, int clientId, int streamId, int streamType){
//
//    int dstSiteId = -1;
//    int dstSiteDegree = siteQosDegree[srcSiteId];
//    int dstSiteScore = 0;
//
//    for(int s=0;s<siteNum;s++){
//        int siteId = siteIdsByQosDegree[s];
//        if(!qosMatrix[siteId][clientId]) continue;
//        if(srcSiteId==siteId) continue;
//        if(maxAvaiList[siteId] - dailyBandwidthFlow[timeId][siteId]< clientNeedSum) continue;
//        int cost = 0;
//        if(siteMaxTypeNeed[siteId][streamType]<=clientNeedSum)
//            cost = siteMaxTypeNeed[siteId][streamType];
//        else
//            cost = clientNeedSum - siteMaxTypeNeed[siteId][streamType];
//
//
//        if(dstSiteScore<gain-cost){
//            dstSiteScore = gain-cost;
//            dstSiteId = siteId;
//            dstSiteDegree = siteQosDegree[dstSiteId];
//        }
////        if(dstSiteScore==gain-cost){
////            if(dstSiteScore%2){
////                if(siteQosDegree[siteId]<dstSiteDegree){
////                    dstSiteId = siteId;
////                    dstSiteDegree = siteQosDegree[dstSiteId];
////                }
////            }
////            else{
////                if(siteQosDegree[siteId]>dstSiteDegree){
////                    dstSiteId = siteId;
////                    dstSiteDegree = siteQosDegree[dstSiteId];
////                }
////            }
////        }
//    }
//    return dstSiteId;
//}
//
//void freshCacheForMig(int siteId, int timeId){
//    int baseTimeId = timeId;
//    for(int newTimeId=timeId;newTimeId<timeNum-1&&newTimeId<baseTimeId+10;newTimeId++){
//        int cache = (int)floor((dailyBandwidthFlow[newTimeId][siteId]+dailyBandwidthCache[newTimeId][siteId])*cacheRate[newTimeId][siteId]);
//
//        dailyBandwidthCache[newTimeId+1][siteId] = cache;
//    }
//}
//
//
//void migrate(int timeId){
//    for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
//        int streamType = streamId - streamDailyBegin[timeId];
//        dailyStreamPair[streamType].first = 0;
//        for(int clientId=0;clientId<clientNum;clientId++){
//            dailyStreamPair[streamType].first += dailyNeeds[clientId][streamType];
//        }
//        dailyStreamPair[streamType].second = streamType;
//    }
//    sort(dailyStreamPair,dailyStreamPair+streamDailyBegin[timeId+1]-streamDailyBegin[timeId],
//         [](pair<int,int>& p1, pair<int,int>& p2){
//             return p1.first > p2.first;
//    });
//
//    for(int p=0;p<streamDailyBegin[timeId+1]-streamDailyBegin[timeId];p++){
//        int streamType = dailyStreamPair[p].second;
//        int streamId = streamType + streamDailyBegin[timeId];
//        for(int clientId=0;clientId<clientNum;clientId++){
//            clientPair[clientId].second = clientId;
//            clientPair[clientId].first = dailyNeeds[clientId][streamType];
//        }
//        sort(clientPair,clientPair+clientNum);
//        for(int c=0;c<clientNum;c++){
//            int clientNeedSum = clientPair[c].first;
//            int clientId = clientPair[c].second;
//            if(clientPair[c].first==0)
//                continue;
//            int srcSiteId = ans[timeId][clientId][streamType];
//
//
//            int otherSiteCost = 0;
//
//            for(int cc=0;cc<clientNum;cc++){
//                if(cc!=clientId&&allocation[srcSiteId][streamType][cc]){
//                    if(dailyNeeds[cc][streamType]>otherSiteCost)
//                        otherSiteCost = dailyNeeds[cc][streamType];
//                }
//            }
//
//            int gain = 0;
//            if(otherSiteCost < clientNeedSum)
//                gain = clientNeedSum - otherSiteCost;
//
//            int dstSiteId = findDstSite(gain, clientNeedSum, srcSiteId, timeId, clientId, streamId, streamType);
//            if(dstSiteId==-1) continue;
//
//            dailyBandwidthFlow[timeId][srcSiteId] -= clientNeedSum;
//            dailyBandwidthFlow[timeId][dstSiteId] += clientNeedSum;
//            allocation[srcSiteId][streamType][clientId] = false;
//            allocation[dstSiteId][streamType][clientId] = true;
//            ans[timeId][clientId][streamType] = dstSiteId;
//
//            int mmmm = 0;
//            for(int c=0;c<clientNum;c++){
//                if(allocation[dstSiteId][streamType][c]&&dailyNeeds[c][streamType]>mmmm)
//                    mmmm = dailyNeeds[c][streamType];
//            }
//            siteMaxTypeNeed[dstSiteId][streamType] = mmmm;
//            mmmm = 0;
//            for(int c=0;c<clientNum;c++){
//                if(allocation[srcSiteId][streamType][c]&&dailyNeeds[c][streamType]>mmmm)
//                    mmmm = dailyNeeds[c][streamType];
//            }
//            siteMaxTypeNeed[srcSiteId][streamType] = mmmm;
//        }
//    }
//    for(int siteId=0;siteId<siteNum;siteId++){
//        freshCacheForMig(timeId,siteId);
//    }
//}
//
//pair<int,int> timeCenterPair[maxTimeNum];
//
//
//bool judgeMaxAvaiByTop06(int maxAvai, int timeId, int siteId, int siteTop06){
//    int curTimeId = timeId;
//    int curCache = maxAvai;
//    while(true){
//        if(curTimeId>=timeNum||curCache==0) break;
//        if(dailyBandwidthFlow[curTimeId][siteId]+curCache>(visitedTop5Days[siteId][curTimeId]?siteBandwidths[siteId]:siteTop06))
//            return false;
//        curCache = (int)floor(curCache*cacheRate[curTimeId][siteId]);
//        curTimeId++;
//    }
//    return true;
//}
//int findMaxAvaiByTop06(int currentAvai, int timeId, int siteId, int interval, int siteTop06){
//    int maxAvai = 0;
//    while(true){
//        if(currentAvai<=0) break;
//        if(judgeMaxAvaiByTop06(currentAvai,timeId,siteId, siteTop06)){
//            maxAvai = currentAvai;
//            break;
//        }
//        currentAvai -= interval;
//    }
//    return maxAvai;
//}
//
//
//
//
//void phase3_run(){
//    LOG_INFO("phase3 begin")
//
//
//    dailyBandwidthFlow = new int*[timeNum];
//    for(int t=0;t<timeNum;t++)
//        dailyBandwidthFlow[t] = new int[siteNum]();
//    dailyBandwidthCache = new int*[timeNum];
//    for(int t=0;t<timeNum;t++)
//        dailyBandwidthCache[t] = new int[siteNum]();
//
//
//
//    for(int timeId=0;timeId<timeNum;timeId++){
//        for(int clientId=0;clientId<clientNum;clientId++){
//            for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
//                int streamType= streamId - streamDailyBegin[timeId];
//                int siteId = ans[timeId][clientId][streamType];
//                int need = streamInitNeeds[streamId][clientId];
//                if(need==0) continue;
//                dailyBandwidthFlow[timeId][siteId] += need;
//            }
//        }
//    }
//
//    for(int siteId=0;siteId<siteNum;siteId++){
//        for(int timeId=1;timeId<timeNum;timeId++){
//            dailyBandwidthCache[timeId][siteId] = (int)floor((dailyBandwidthFlow[timeId-1][siteNum]+dailyBandwidthCache[timeId-1][siteNum])*cacheRate[timeId-1][siteId]);
//        }
//    }
//
//    int* banwidthTemp = new int[timeNum];
//    for(int siteId=0;siteId<siteNum;siteId++){
//        for(int timeId=0;timeId<timeNum;timeId++){
//            banwidthTemp[timeId] = dailyBandwidthFlow[timeId][siteId];
//        }
//        sort(banwidthTemp,banwidthTemp+timeNum);
//        top06List[siteId] = banwidthTemp[timeNum95-1];
//        if(banwidthTemp[timeNum-1]>0&&top06List[siteId]<baseCost)
//            top06List[siteId] = baseCost;
//    }
//
//
//    int* siteMaxNeed = new int[siteNum]();
//    int* timeNeeds = new int[timeNum]();
//    for(int timeId=0;timeId<timeNum;timeId++){
//        int maxNeedSum = 0;
//        int needSum = 0;
//        for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
//            int streamType = streamId - streamDailyBegin[timeId];
//            for(int s=0;s<siteNum;s++)
//                siteMaxNeed[s] = 0;
//            for(int clientId=0;clientId<clientNum;clientId++){
//                int siteId = ans[timeId][clientId][streamType];
//                if(siteId==-1) continue;
//                siteMaxNeed[siteId] = max(siteMaxNeed[siteId], streamInitNeeds[streamId][clientId]);
//                needSum += streamInitNeeds[streamId][clientId];
//            }
//            for(int s=0;s<siteNum;s++)
//                maxNeedSum += siteMaxNeed[s];
//        }
//        timeCenterPair[timeId].first = maxNeedSum;
//        timeCenterPair[timeId].second = timeId;
//        timeNeeds[timeId] = needSum;
//    }
//    sort(timeCenterPair,timeCenterPair+timeNum,
//         [](pair<int,int>& p1, pair<int,int>& p2){
//             return p1.first > p2.first;
//         });
//
//    dailyNeeds = new int*[clientNum];
//    for(int c=0;c<clientNum;c++)
//        dailyNeeds[c] = new int[maxDailyStreamTypeNum]();
//
//
//    allocation = new bool**[siteNum];
//    for(int s=0;s<siteNum;s++){
//        allocation[s] = new bool*[maxDailyStreamTypeNum];
//        for(int p=0;p<maxDailyStreamTypeNum;p++)
//            allocation[s][p] = new bool[clientNum]();
//    }
//
//    siteMaxTypeNeed = new int*[siteNum];
//    for(int s=0;s<siteNum;s++)
//        siteMaxTypeNeed[s] = new int[maxDailyStreamTypeNum]();
//
//
//
//
//    for(int t=0;t<timeNum;t++){
//        int timeId = timeCenterPair[t].second;
//        // update maxAvai
//        for(int s=0;s<siteNum;s++){
//            int siteId = siteIdsByQosDegree[s];
//            if(siteQosDegree[siteId]==0)continue;
//            int maxAvai = siteBandwidths[siteId] - dailyBandwidthCache[timeId][siteId];;
//            maxAvai = findMaxAvaiByTop06(maxAvai, timeId, siteId, 1, top06List[siteId]);
//            maxAvai = max(maxAvai-100,0);
//            maxAvaiList[siteId] = maxAvai;
//        }
//
//        for(int clientId=0;clientId<clientNum;clientId++){
//            for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
//                int streamType = streamId - streamDailyBegin[timeId];
//                //cout<<streamType<<' '<<timeId<<' '<<clientId<<' '<<streamId<<endl;
//                dailyNeeds[clientId][streamType] = streamInitNeeds[streamId][clientId];
//            }
//        }
//
//        for(int clientId=0;clientId<clientNum;clientId++){
//            for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
//                int streamType = streamId - streamDailyBegin[timeId];
//                int siteId = ans[timeId][clientId][streamType];
//                if(siteId==-1)continue;
//                allocation[siteId][streamType][clientId] = true;
//            }
//        }
//
//
//
//        for(int siteId=0;siteId<siteNum;siteId++){
//            for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
//                int maxNeed = 0;
//                int streamType = streamId - streamDailyBegin[timeId];
//                for(int clientId=0;clientId<clientNum;clientId++){
//                    if(allocation[siteId][streamType][clientId])
//                        maxNeed = max(maxNeed, dailyNeeds[clientId][streamType]);
//                }
//                siteMaxTypeNeed[siteId][streamType] = maxNeed;
//            }
//        }
//
//
//
//        for(int iter=0;iter<4;iter++){
//            migrate(timeId);
//        }
//
//    }
//
//    LOG_INFO("phase3 end")
//
//
//
//}


void output(){
    string ansStr = "";
    for(int timeId=0;timeId<timeNum;timeId++){
        if(timeId>0){

            int cnt = 0;
            for(int siteId=0;siteId<siteNum;siteId++){
                if(cacheRate[timeId-1][siteId]<0.04){
                    if(cnt>0) ansStr += ",";
                    ansStr += siteNames[siteId];
                    cnt++;
                    if(cnt==20)break;
                }
            }
            ansStr += "\n";
        }
        
        for(int clientId=0;clientId<clientNum;clientId++){
            ansStr += clientNames[clientId];
            ansStr += ":";
            bool firstFlag = true;
            for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
                int streamIndex = streamId - streamDailyBegin[timeId];
                int siteId = ans[timeId][clientId][streamIndex];
                if(siteId==-1) continue;
                if(!firstFlag)
                    ansStr += ",";
                ansStr += ("<"+siteNames[siteId]+","+streamNames[streamId]+">");
                if(firstFlag)
                    firstFlag = false;
            }
            ansStr += "\n";
        }
    }
    
    size_t length = ansStr.length();
    int fd = open(solutionPath.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    char* out = (char*)mmap(NULL,length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    lseek(fd, length-1, SEEK_SET);
    write(fd, "\0", 1);
    memcpy(out, ansStr.c_str(), length);
    munmap(out, length);
}


void evaluate(){
    
    int** siteFlow;
    siteFlow = new int*[siteNum];
    for(int s=0;s<siteNum;s++)
        siteFlow[s] = new int[timeNum]();
    
    int*** siteTimeTypeMaxFlow;
    siteTimeTypeMaxFlow = new int**[siteNum];
    for(int s=0;s<siteNum;s++){
        siteTimeTypeMaxFlow[s] = new int*[timeNum];
        for(int t=0;t<timeNum;t++)
            siteTimeTypeMaxFlow[s][t] = new int[maxDailyStreamTypeNum]();
    }
    int* centerFlow = new int[timeNum]();
    
    bool missFlag = false;
    
    for(int timeId=0;timeId<timeNum;timeId++){
        for(int clientId=0;clientId<clientNum;clientId++){
            for(int streamId=streamDailyBegin[timeId];streamId<streamDailyBegin[timeId+1];streamId++){
                int streamType= streamId - streamDailyBegin[timeId];
                int siteId = ans[timeId][clientId][streamType];
                int need = streamInitNeeds[streamId][clientId];
                if(need==0) continue;
                if(siteId==-1&&need>0){
                    if(!missFlag){
                        cout<<"miss allocation"<<endl;
                        missFlag = true;
                    }
                    continue;
                    //exit(0);
                }
                siteFlow[siteId][timeId] += need;
                siteTimeTypeMaxFlow[siteId][timeId][streamType] = max(need, siteTimeTypeMaxFlow[siteId][timeId][streamType]);
            }
        }
    }
    double siteCostSum = 0;
    for(int siteId=0;siteId<siteNum;siteId++){
        int siteFlowSum = siteFlow[siteId][0];
        for(int timeId=1;timeId<timeNum;timeId++){
            siteFlow[siteId][timeId] += (int)floor(siteFlow[siteId][timeId-1]*cacheRate[timeId-1][siteId]);
            siteFlowSum += siteFlow[siteId][timeId];
        }
        sort(siteFlow[siteId]+0,siteFlow[siteId]+timeNum);
        int siteFlow95 = siteFlow[siteId][timeNum95-1];
        
        double siteCost = 0;
        if(siteFlowSum>0){
            if(siteFlow95>=0&&siteFlow95<=baseCost){
                siteCost = baseCost;
            }
            else{
                siteCost = pow(siteFlow95-baseCost,2)*1.0/siteBandwidths[siteId]+siteFlow95;
            }
        }
        siteCostSum += siteCost;
    }
    
    for(int siteId=0;siteId<siteNum;siteId++){
        for(int timeId=0;timeId<timeNum;timeId++){
            for(int streamType=0;streamType<maxDailyStreamTypeNum;streamType++){
                centerFlow[timeId] += siteTimeTypeMaxFlow[siteId][timeId][streamType];
            }
        }
    }
    sort(centerFlow,centerFlow+timeNum);
    int centerFlow95 = centerFlow[timeNum95-1];
    double centerCostSum = centerCost*centerFlow95;
    
    int totalCost = (int)floor(siteCostSum+centerCostSum+0.5);
    
    
    LOG_INFO("siteCostSum: %.2lf", siteCostSum)
    LOG_INFO("centerCostSum: %.2lf", centerCostSum)
    LOG_INFO("totalCost: %d", totalCost)

    
}



int main(int argc, const char * argv[]) {
    readData();
    prepare();
    phase0_run();
    phase1_run();
    phase2_run();
    //phase3_run();

    output();
#ifdef OFFLINE
    evaluate();
#endif
    return 0;
}

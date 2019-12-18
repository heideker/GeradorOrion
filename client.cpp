/* 
 * File:   client.cpp
 * Author: Alexandre Heideker
 *
 * Created on October,04 - 2019
 */

#include <stack>
#include <cstdlib>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <thread>
#include <unistd.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <cstring>
#include <thread>
#include <iostream>
#include <curl/curl.h>

#include "swissknife.h"


#define SEM_WAIT mt.lock();
#define SEM_POST mt.unlock();
#define SEM_JOB_WAIT mtJob.lock();
#define SEM_JOB_POST mtJob.unlock();
#define QTD_WORKERS 100
using namespace std;

unsigned RndI(int, int);
void Randomize(void);
void SleepExp(float);
float getTimeExp(float);
void Worker(int);
bool deleteEntity(unsigned EnttId);
bool createEntity(unsigned EnttId);
string updateEntity(unsigned EnttId);
bool getRestFiware(string url, curl_slist *chunk, string data, string request, string &bufferOut);
size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp);
bool readSetup();
bool readSetupFromCL(int argc, char *argv[]);
bool parseVar(string token, string value);
void dumpVar();
double getNow();

//settings
float alfa;
string target;
string tag;
int id;
int ExpTime;
int _debugMode = 0;
unsigned NumberOfSensors;
int qtdWorkers = 10;
bool logSql = false;
bool noCreate = false;
bool pubJson = false;
bool singleAlfa = false;
//globals
int ExpStart;
float rtt;
std::mutex mt;
std::mutex mtJob;
//stack<int> Jobs;
stack<int> ThrStack;
stack<int> ExecStack;
bool doWork = true;
string FileName;
bool comma = false;

long TimeOut = 30L;
long connectTimeOut = 10L;

ofstream logOut;



int main(int argc, char *argv[]) {
    //char s[200];
    rtt = 0;
    Randomize();
    readSetup();
    if (argc>1) {
        if (!readSetupFromCL(argc, argv)) return -1;
    }
    if (_debugMode>1) dumpVar();
    char f[30];
    sprintf(f, "client%04u.txt", id);
    FileName = f;
    logOut.open(FileName.c_str(), ios_base::out);
    if (logSql) logOut << "insert into client (exp, tStamp, delay, erro) values " << endl;
    if (_debugMode>0) cout << "Create log file: " << FileName << endl;
    std::thread * th_id = new std::thread[qtdWorkers];
 
    if (_debugMode>0) cout << "Start...." << endl;
    if (!noCreate) {
        for (unsigned i=0; i<NumberOfSensors; i++){
                if (_debugMode>0) cout << "\r" << "Deletting entity " << i << std::flush;
                deleteEntity(i);
        }
        if (_debugMode>0) cout << "...OK" << endl;

        if (_debugMode>0) cout << "Wait some time to stabilize system...." << endl;
        sleep(20);

        //creating sensors...

        for (unsigned i=0; i<NumberOfSensors; i++){
            if (_debugMode>0) cout << "\r" << "Creating entity " << i << std::flush;
            createEntity(i);
        }
        if (_debugMode>0) cout << "...OK" << endl;
    }
    //create workers
    cout << endl;
    if (!singleAlfa) alfa = alfa / qtdWorkers;
    sleep(20);
    for (int i=0; i<qtdWorkers; i++){
        if (_debugMode>0) cout << "\r" << "Startting worker " << i << std::flush;
        th_id[i] = thread(Worker, i);
    }
    if (_debugMode>0) cout << "...OK" << endl;

    if (_debugMode>0) cout << "Finalizing the experiment..." << endl;
    doWork = false;
    for (int i = 0; i < qtdWorkers; i++)
        th_id[i].join();
    logOut.close();
    return 0;
}


void Worker(int n) {
    ExpStart = (unsigned) time(NULL);
    unsigned t0 = (unsigned) time(NULL);
    float alfaT = alfa * .1;
    unsigned t;
    unsigned t1 = (ExpTime * .1);
    unsigned t2 = (ExpTime * .9);

    float alfa0 = alfa * .1;
    float alfa2 = alfa;
    int i = 0;
    float timeExp = getTimeExp(alfa0);
    float step = ((.9 * alfa) / (.8 * ExpTime));

    while ((ExpStart + ExpTime - (unsigned) timeExp) >= (unsigned) time(NULL)) {
        SleepExp(timeExp);
        unsigned n = RndI(0, NumberOfSensors);

        //update timestamp from entity n
        if (_debugMode>1) cout << "Updating entity..." << endl;
        double ti = getNow();
        unsigned tInicio =  (unsigned) time(NULL) ;
        string r = updateEntity(n);

        ti = getNow() - ti;

        //write statistics
        SEM_WAIT
        if (_debugMode>1) cout << "Writing statistics..." << endl;
        //ofstream log;
        //log.open(FileName.c_str(), ios_base::app);
        if (logSql) {
            logOut << ((comma)? ",":"") << " ('" << tag << "', " << tInicio 
                << ", " << ti << ", " << ((r=="OK")? "0" : "1" ) << ")" << std::flush;
            comma = true;
        } else {
            logOut << id << ";" << tag << ";" << tInicio << ";" << i << ";"
                << ti << ";" << r << endl;
        }
        //log.close();
        SEM_POST

        t = (unsigned) time(NULL) - t0;
        if (t < t1) {
            alfaT = alfa0;
        } else if (t > t2) {
            alfaT = alfa2;
        } else {
            alfaT = alfa0 + (t * step) ;
        }
        i++;
        timeExp = getTimeExp(alfaT);
    }
    if (_debugMode>0) cout << "\rWorker " << n << " good bye!    " << std::flush;
}


void Randomize(void) {
    srand(time(NULL));
}

unsigned RndI(int min, int max) {
    unsigned k;
    double d;
    d = (double) rand() / ((double) RAND_MAX + 1);
    k = d * (max - min + 1);
    return min + k;
}

void SleepExp(float e) {
    int s;
    long ns;
    float resto;
    resto = (float) e - (int) e;
    s = (int) e;
    ns = (long) (resto * 1000000000L);
    struct timespec req = {0};
    req.tv_sec = s;
    req.tv_nsec = ns;
    nanosleep(&req, (struct timespec *)NULL);    
    //nanosleep((struct timespec[]) {{s, ns}}, NULL);
    return;
}

float getTimeExp(float alf) {
    float r;
    float e;
    r = (float) rand() / ((float) RAND_MAX + 1);
    e = -1 * log(1 - r) / alf;
    return e;
}

bool createEntity(unsigned EnttId){
    ostringstream json;

    json << "{\"id\": \"sensor-1-" << EnttId << "\",\"type\": \"Sensor\", \"timestamp\": {\"value\":" << 
        (unsigned) std::time(0) << ", \"type\": \"Integer\"}}";

    ostringstream url;

    url << target << ":1026/v2/entities/"; //sensor-1-" << EnttId;
    if (_debugMode>1) cout << "URL:\t" << url.str() << endl;
    if (_debugMode>1) cout << "JSON:\t" << json.str() << endl;
    struct curl_slist *chunk = NULL;
    chunk = curl_slist_append(chunk, "Content-Type: application/json");
    chunk = curl_slist_append(chunk, "Accept: application/json");

    string retStr;
    if (getRestFiware(url.str(), chunk, json.str(), "", retStr)) { 
    	if (retStr.length()>0)
            if (retStr.find("{\"error\":",0)) 
            	return false;
    } else return false;
    return true;
}

string updateEntity(unsigned EnttId){
    ostringstream url;
    struct curl_slist *chunk = NULL;
    string retStr;
    if (pubJson) {
        ostringstream json;
        json << "{\"timestamp\": {\"value\":" << (unsigned) std::time(0) << ", \"type\": \"Integer\"}}";
        url << target << ":1026/v2/entities/sensor-1-" << EnttId << "/attrs";
        if (_debugMode>1) cout << "URL:\t" << url.str() << endl;
        chunk = curl_slist_append(chunk, "Content-Type: application/json"); 
        if (!getRestFiware(url.str(), chunk, json.str(), "PATCH", retStr)) return "ERROR";
    } else {
        url << target << ":1026/v2/entities/sensor-1-" << EnttId << "/attrs/timestamp/value";
        if (_debugMode>1) cout << "URL:\t" << url.str() << endl;
        chunk = curl_slist_append(chunk, "Content-Type: text/plain"); 
        if (!getRestFiware(url.str(), chunk, to_string( (unsigned) std::time(0)), "PUT", retStr)) return "ERROR";
    }
    if (_debugMode>2) cout << "### Return: " << retStr << endl;
    if (retStr.length()>0)
        if (retStr.find("{\"error\":",0)) 
            return "ERROR";
    return "OK";
}


bool deleteEntity(unsigned EnttId){
    ostringstream url;

    url << target << ":1026/v2/entities/sensor-1-" << EnttId;
    if (_debugMode>1) cout << "URL:\t" << url.str() << endl;
    struct curl_slist *chunk = NULL;
    //chunk = curl_slist_append(chunk, "Content-Type: application/json");
    chunk = curl_slist_append(chunk, "Accept: application/json");

    string retStr;
    if (getRestFiware(url.str(), chunk, "", "DELETE", retStr)) {
        if (retStr.find("{\"error\":",0)) 
            return false;
    } else return false;
    return true;
}

bool getRestFiware(string url, curl_slist *chunk, string data, string request, string &bufferOut) {
    CURL *curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        if (_debugMode>2) curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        if (data != "") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
            if (_debugMode>2) cout << "### Data: " << data << endl;
        }
        string Buffer;
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, TimeOut); //timeout = 30s
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeOut); //connection timeout = 30s
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &Buffer);
        if (request!="") curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.c_str());
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            curl_easy_cleanup(curl);
            curl_global_cleanup();
	    bufferOut = "";
	    return false;
            //cout << "erro" << endl;
            //return "";
        }
        curl_easy_cleanup(curl);
        curl_global_cleanup();
	bufferOut = Buffer;
        return true;
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    bufferOut = "";
    return false;
    //cout << "erro" << endl;
    //return "";
}

size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)	{
	    ((std::string*)userp)->append((char*)contents, size * nmemb);
	    return size * nmemb;
}


bool parseVar(string token, string value){
    if (token == "debugMode") {
        _debugMode = stoi(value);
    } else
    if (token == "logSql") {
        logSql = true;
    } else
    if (token == "noCreate") {
        noCreate = true;
    } else
    if (token == "pubJson") {
        pubJson = true;
    } else
    if (token == "alfa") {
        alfa = stof(value);
    } else
    if (token == "connectTimeOut") {
        connectTimeOut = stol(value);
    } else
    if (token == "TimeOut") {
        TimeOut = stol(value);
    } else
    if (token == "singleAlfa") {
        singleAlfa = true;
    } else
    if (token == "target") {
        target = value;
    } else
    if (token == "tag") {
        tag = value;
    } else
    if (token == "id") {
        id = stoi(value);
    } else
    if (token == "ExpTime") {
        ExpTime = stoi(value);
    } else
    if (token == "qtdWorkers") {
        qtdWorkers = stoi(value);
    } else
    if (token == "NumberOfSensors") {
        NumberOfSensors =  (unsigned) stoi(value);
    } else
    if (token == "help") {
        cout << "Usage: \n \t --debugMode=<int>\n\t--alfa=<float>\n\t--connectTimeOut=n\n\t--TimeOut=n\n\t--target=<host>\n\t--tag=<exp.tag>\n\t--id=<int>\n\t--ExpTime=<int>\n\t--NumberOfSensors=<int>\n\t--qtdWorkers=<int>\n\t--logSQL\n\t--pubJson\n\t--help (this help)" << endl;
        return false;
    } else
    {
        cout << "Invalid argument: Token=" << token << "  Value=" << value << endl;
        cout << "Usage: \n \t --debugMode=1\n\t--alfa=<float>\n\t--connectTimeOut=n\n\t--TimeOut=n\n\t--target=<host>\n\t--tag=<exp.tag>\n\t--id=<int>\n\t--ExpTime=<int>\n\t--NumberOfSensors=<int>\n\t--qtdWorkers=<int>\n\t--logSQL\n\t--pubJson\n\t--help (this help)" << endl;
        return false;
    }
    return true;
}

bool readSetup(){
    bool error = false;
    ifstream File;
    string line;
    File.open ("setup.conf");
    if (File.is_open()) {
        string token;
        string value;
        while (!File.eof() && !error){
            getline(File, line);
            if (line[0] != '#' && line!="") {
                token = trim(line.substr(0, line.find("=")));
                value = trim(line.substr(line.find("=")+1, line.length()-1));
                error = !parseVar(token, value);
            }
        }
        File.close();
        return !error;
    } else {
        return false;
    }
}

bool readSetupFromCL(int argc, char *argv[]){
    int i;
    bool error = false;
    string token;
    string value;
    for (i=1; i<argc; i++) {
        string line(argv[i]);
        if (line[0] == '-' && line[1] == '-') {
            token = trim(line.substr(2, line.find("=")-2));
            value = trim(line.substr(line.find("=")+1, line.length()-1));
            error = !parseVar(token, value);
        } else {
            error = true;
        }
        if (error) break;
    }
    if (error) {
        return false;
    } else {
        return true;
    }
}

void dumpVar() {
    cout << "*** DEBUG Mode *** Dumping variables:" << endl;
    cout << "Alfa:\t" << alfa << endl;
    cout << "Target:\t" <<  target << endl;
    cout << "Experiment Tag:\t" <<  tag << endl;
    cout << "Experiment Id:\t" <<  id << endl;
    cout << "Experiment Time:\t" <<  ExpTime << endl;
    cout << "Number of Sensors:\t" <<  NumberOfSensors << endl;
    cout << "Number of Threads (workers):\t" <<  qtdWorkers << endl;
}

double getNow(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double) (tv.tv_sec) + (double)(tv.tv_usec) / 1000000;
}


#include <curl/curl.h>
#include <string>
using namespace std;
class Rest {

private:
	string data;
	static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)	{
	    ((std::string*)userp)->append((char*)contents, size * nmemb);
	    return size * nmemb;
	}
    double getNow(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (double) (tv.tv_sec) + (double)(tv.tv_usec) / 1000000;
    }

public:
    typedef struct {
        bool ok;
        long size;//bytes
        double time; //seconds
        double throughput; //Mbits/s
    } wget_status;

    wget_status wget(string url) {
        double ti = getNow();
        wget_status st;
        CURL* curl;
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &Rest::writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        
        if (curl_easy_perform(curl)==CURLE_OK) {
            st.ok = true;
            st.size = data.size();
            st.time = getNow() - ti;
            st.throughput = (st.time>0? ((st.size*8)/st.time)/1000000L: 0);
            curl_easy_cleanup(curl);
        } else {
            st.ok=false;
            st.size = 0;
            st.time = 0;
            st.throughput = 0;
        }
        curl_global_cleanup();
        return st;
    }

	string get(string url) {
    	CURL* curl;
	    curl_global_init(CURL_GLOBAL_ALL);
	    curl = curl_easy_init();
    	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &Rest::writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	    curl_easy_perform(curl);
	    curl_easy_cleanup(curl);
	    curl_global_cleanup();
	    return data;
	}

	string getPost(string url, string postfields) {
    	CURL* curl;
	    curl_global_init(CURL_GLOBAL_ALL);
	    curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields.c_str());
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &Rest::writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	    curl_easy_perform(curl);
	    curl_easy_cleanup(curl);
	    curl_global_cleanup();
	    return data;
	}

        string getPost(string url, string postfields, string request) {
            CURL* curl;
	    curl_global_init(CURL_GLOBAL_ALL);
	    curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields.c_str());
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &Rest::writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	    curl_easy_perform(curl);
	    curl_easy_cleanup(curl);
	    curl_global_cleanup();
	    return data;
	}

        

        
	bool post(string url, string postfields) {
		CURL *curl;
		CURLcode res;
		curl_global_init(CURL_GLOBAL_ALL);
		curl = curl_easy_init();
		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields.c_str());
			res = curl_easy_perform(curl);
			if(res != CURLE_OK) {
				curl_easy_cleanup(curl);
				curl_global_cleanup();
				return false;
			}
			curl_easy_cleanup(curl);
			curl_global_cleanup();
			return true;
		}
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		return false;
	}

};

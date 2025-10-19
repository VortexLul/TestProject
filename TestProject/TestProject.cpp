#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>
#include <regex>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>

using namespace std;


struct ResponseData {
    string content;
    string contentDisposition;
    long responseCode;
};

struct DownloadTask {
    string url;
    string directoryPath;
    int taskId;
};

queue<DownloadTask> taskQueue;
mutex queueMutex;
condition_variable condition;
atomic<bool> stopThreads{ false };
atomic<int> activeThreads{ 0 };
atomic<int> completedTasks{ 0 };
atomic<int> failedTasks{ 0 };


size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* data) {
    size_t total_size = size * nmemb;
    data->append((char*)contents, total_size);
    return total_size;
}

size_t HeaderCallback(void* contents, size_t size, size_t nmemb, ResponseData* response ) {
    size_t total_size = size * nmemb;
    string header((char*)contents, total_size);
    
    if (header.find("Content-Disposition:") != string::npos) {
        response->contentDisposition = header;
    }
    return total_size;
}





string ExtractFileName(const string& contentDisposition) {
    if (contentDisposition.empty()) {
        return "";
    }

    size_t filename_pos = contentDisposition.find("filename=");
    if (filename_pos == string::npos) {
        filename_pos = contentDisposition.find("filename =");
    }
    if (filename_pos == string::npos) {
        filename_pos = contentDisposition.find("Filename=");
    }
    if (filename_pos == string::npos) {
        return "";
    }


    filename_pos += 9;
    while (filename_pos < contentDisposition.length() &&
        (contentDisposition[filename_pos] == ' ' || contentDisposition[filename_pos] == '\t')) {
        filename_pos++;
    }

    string filename;
    
    if (filename_pos < contentDisposition.length() &&
        (contentDisposition[filename_pos] == '\"' || contentDisposition[filename_pos] == '\'')) {
        filename_pos++;
        size_t end_quote = contentDisposition.find(contentDisposition[filename_pos - 1], filename_pos);
        if (end_quote != std::string::npos) {
            filename = contentDisposition.substr(filename_pos, end_quote - filename_pos);
        }
    }
    else {
        size_t end_pos = contentDisposition.length();
        for (size_t i = filename_pos; i < contentDisposition.length(); i++) {
            if (contentDisposition[i] == ';' || contentDisposition[i] == '\r' ||
                contentDisposition[i] == '\n' || contentDisposition[i] == ' ' ||
                contentDisposition[i] == '\t') {
                end_pos = i;
                break;
            }
        }
        if (end_pos > filename_pos) {
            filename = contentDisposition.substr(filename_pos, end_pos - filename_pos);
        }
    }

    if (!filename.empty()) {
        size_t start = filename.find_first_not_of(" \t\r\n");
        size_t end = filename.find_last_not_of(" \t\r\n");
        if (start != string::npos && end != string::npos) {
            filename = filename.substr(start, end - start + 1);
        }
    }
    return filename;

}

string ExtractFileNameFromUrl(const string& url) {

    size_t question_mark = url.find('?');
    string clean_url = (question_mark != string::npos) ? url.substr(0, question_mark) : url;

    size_t hash_mark = clean_url.find('#');
    if (hash_mark != string::npos) {
        clean_url = clean_url.substr(0, hash_mark);
    }

    size_t last_slash = clean_url.find_last_of('/');
    if (last_slash != string::npos && last_slash + 1 < clean_url.length()) {
        string filename = clean_url.substr(last_slash + 1);

        if (!filename.empty() && filename.find('.') != string::npos) {
            return filename;
        }
    }

    return "downloaded_file";
}



string ReplaceUnvalidName(const string& filename) {
    if (filename.empty()) {
        return "dowloaded_file";
    }

    string replace = filename;

    regex invalid_chars("[<>:\"/\\\\|?*]");
    replace = regex_replace(replace, invalid_chars, "_");

    size_t start = replace.find_first_not_of(" .");
    size_t end = replace.find_last_not_of(" .");

    if (start != string::npos && end != string::npos) {
        replace = replace.substr(start, end - start + 1);
    }
    else {
        replace = "downloaded_file";
    }

    if (replace.empty()) {
        return "download_file";
    }

    return replace;
}

string UniqueFileName(const filesystem::path& directory, const string& filename) {
    filesystem::path basePath = directory / filename;

    if (!filesystem::exists(basePath)) {
        return basePath.string();
    }

    string stem = basePath.stem().string();
    string extension = basePath.extension().string();

    if (extension.empty()) {
        int counter = 1;
        while (true) {
            filesystem::path newPath = directory / (stem + " (" + to_string(counter) + ")");
            if (!filesystem::exists(newPath)) {
                return newPath.string();
            }
            counter++;
        }
    }
    else {
        
        int counter = 1;
        while (true) {
            filesystem::path newPath = directory / (stem + " (" + to_string(counter) + ")" + extension);
            if (!filesystem::exists(newPath)) {
                return newPath.string();
            }
            counter++;
        }
    }
}

bool DowloadFunc(const string & url, string & folderPath, int taskId) {
        CURL* curl;
        CURLcode res;
        ResponseData response;

        curl = curl_easy_init();
        if (!curl) {
            cerr << "[Task" << taskId << "]Fail init ������ �������������" << endl;
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.content);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");


        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            cerr << "[Task" << taskId << "]Fail DOWNL ������ ����������:" << curl_easy_strerror(res) << endl;
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.responseCode);
        curl_easy_cleanup(curl);

        if (response.responseCode != 200) {
            cerr << "[Task" << taskId << "]Fail HTTP ������ HTTP ������" << response.responseCode << "��� URL" << url << endl;
            curl_easy_cleanup(curl);
            return false;
        }

        string filename;

        if (!response.contentDisposition.empty()) {
            filename = ExtractFileName(response.contentDisposition);
        }

        if (filename.empty()) {
            filename = ExtractFileNameFromUrl(url);
        }

        filename = ReplaceUnvalidName(filename);

        filesystem::path dirpath(folderPath);
        filesystem::create_directories(dirpath);

        string fullPath = UniqueFileName(dirpath, filename);

        ofstream file(fullPath, ios::binary);
        if (!file.is_open()) {
            cerr << "Fail file ������ �������� �����: " << fullPath << endl;
            return false;
        }

        file.write(response.content.c_str(), response.content.size());
        file.close();
        cout << "[������ " << taskId << "]Success Download ���� ������� ������: " << fullPath << endl;
        return true;
    }


void WorkerThread() {
    activeThreads++;

    while (true) {
        DownloadTask;

        {
            unique_lock<std::mutex> lock(queueMutex);
        }

    }




}



int main() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        string url, folderPath;

        cout << "������� URL �����:";
        getline(cin, url);

        cout << "������� ���� ��� ����������:";
        getline(cin, folderPath);

        if (url.empty() || folderPath.empty()) {
            cerr << "������: URL � ���� �� ����� ���� �������" << endl;
            curl_global_cleanup();
            return 1;
        }

        cout << "���������� �����..." << endl;
        cout << "URL: " << url << endl;
        cout << "����: " << folderPath << endl;


        if (DowloadFunc(url, folderPath)) {
            cout << "Suc�ess ���� �������� ������" << endl;
        } else {
            cerr << "Fail ������ ���������� �����" << endl;
            curl_global_cleanup();
            return 1;
        }

        curl_global_cleanup();

        return 0;

        // if (url.empty() || folderPath.empty()) {
        //     cerr << "URL � ���� �� ������ ���� �������" << endl;
        //     return 1;
        // }
        /////* if (DowloadFunc(url, folderPath)) {
        ////      cout << "���� ������� ������!" << std:endl;
        //// }
        //// else {
        ////     std::cerr << "������ ���������� �����" << endl;
        ////     curl_global_cleanup();
        ////     return 1;
        //// }*/
        //// curl_global_cleanup();
        //return 0;
}
   
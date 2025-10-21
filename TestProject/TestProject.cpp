#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
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
    long responseCode = 0;
};

struct DownloadTask {
    string url;
    string directoryPath;
    int taskId = 0;
};


queue<DownloadTask> taskQueue;
mutex qMutex;
condition_variable condition;
atomic<bool> stopThreads{ false };
atomic<int> activeThreads{ 0 };
atomic<int> completedTasks{ 0 };
atomic<int> failedTasks{ 0 };
atomic<int> totalTasks{ 0 };

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
    if (!userdata || !contents || size == 0 || nmemb == 0) {
        return 0;
    }

    string* data = static_cast<string*>(userdata);
    size_t total_size = size * nmemb;

    try {
        data->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    catch (...) {
        return 0;
    }
}

size_t HeaderCallback(void* contents, size_t size, size_t nmemb, void* userdata) {
    if (!userdata || !contents || size == 0 || nmemb == 0) {
        return 0;
    }

    ResponseData* response = static_cast<ResponseData*>(userdata);
    size_t total_size = size * nmemb;

    try {
        string header(static_cast<const char*>(contents), total_size);

        if (header.find("Content-Disposition:") != string::npos) {
            response->contentDisposition = header;
        }
        return total_size;
    }
    catch (...) {
        return 0;
    }
}

string ExtractFileName(const string& contentDisposition) {
    if (contentDisposition.empty()) {
        return "";
    }

    size_t filename_pos = contentDisposition.find("filename=");
    if (filename_pos == string::npos) {
        return "";

    }

    /*    filename_pos = contentDisposition.find("filename =");
    }
    if (filename_pos == string::npos) {
        filename_pos = contentDisposition.find("Filename=");
    }
    if (filename_pos == string::npos) {
        return "";
    }*/


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
    if (url.empty()) return "downloaded_file";

    string clean_url = url;
    size_t question_mark = clean_url.find('?');
    if (question_mark != string::npos) {
        clean_url = clean_url.substr(0, question_mark);
    }
    size_t hash_mark = clean_url.find('#');
    if (hash_mark != string::npos) {
        clean_url = clean_url.substr(0, hash_mark);
    }
    size_t last_slash = clean_url.find_last_of('/');
    if (last_slash != string::npos && last_slash + 1 < clean_url.size()) {
        string filename = clean_url.substr(last_slash + 1);
        if (!filename.empty()) {
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

bool DowloadFunc(const string & url, string & directoryPath, int taskId) {
        CURL* curl = curl_easy_init;
        

        if (!curl) {
            cerr << "[Task" << taskId << "]Ошибка инициализации" << endl;
            return false;
        }

        ResponseData response;
        CURLcode res;

        struct CurlGuard {

        };

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.content);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");


        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            cerr << "[Task" << taskId << "]Ошибка скачивания:" << curl_easy_strerror(res) << endl;
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.responseCode);
        curl_easy_cleanup(curl);

        if (response.responseCode != 200) {
            cerr << "[Task" << taskId << "]Ошибка HTTP ответа" << response.responseCode << "для URL" << url << endl;
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

        filesystem::path dirpath(directoryPath);
        try {
            filesystem::create_directories(dirpath);
        }
        catch (const exception& e) {
            cerr << "[Task " << taskId << "] Ошибка создания директории: " << e.what() << endl;
            return false;
        }

        string fullPath = UniqueFileName(dirpath, filename);

        ofstream file(fullPath, ios::binary);
        if (!file.is_open()) {
            cerr << "Ошибка создания файла: " << fullPath << endl;
            return false;
        }

        file.write(response.content.c_str(), response.content.size());
        file.close();
        std::cout << "[Задача " << taskId << "]Файл успешно скачан: " << fullPath << endl;
        return true;
    }


void WorkerThread() {
    activeThreads++;

    try {
        while (!stopThreads) {
            DownloadTask task;
            bool has_task = false;
            {
                unique_lock<mutex> lock(qMutex);

                if (!taskQueue.empty() && stopThreads) {
                    break;
                }
                if (taskQueue.empty()) {
                    task = move(taskQueue.front());
                    taskQueue.pop();
                    has_task = true;
                }
                else {
                    condition.wait(lock);
                    continue;
                }
            }

            if (has_task) {
                if (DowloadFunc(task.url, task.directoryPath, task.taskId)) {
                    completedTasks++;
                }
                else {
                    failedTasks++;
                }

                int processed = completedTasks + failedTasks;
                if (processed % 10 == 0 || processed == totalTasks) {
                    cout << "[Прогресс] Обработано: " << processed << "/" << totalTasks << " (" << (totalTasks > 0 ? (processed * 100 / totalTasks) : 0)<< "%)" << endl;
                }
            }
        }
    }
    catch (const exception& e) {
        cerr << "Ошибка в потоке: " << e.what() << endl;
        failedTasks++;
    }

    activeThreads--;
}

void AddQueue(const string& url, const string& directoryPath, int taskId) {
    lock_guard<mutex> lock(qMutex);
    taskQueue.push({url, directoryPath, taskId});
    condition.notify_one();
}

vector<string> ReadUrlsFromFile(const string& filename) {
    vector<string> urls;
    ifstream file(filename);

    if (!file.is_open()) {
        throw runtime_error("невозможно открыть файл: " + filename);
    }

    string line;
    while (getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            size_t start = line.find_first_not_of(" \t");
            size_t end = line.find_last_not_of(" \t");
            if (start != string::npos && end != string::npos) {
                urls.push_back(line.substr(start, end - start + 1));
            }
        }
    }

    if (urls.empty()) {
        throw runtime_error("File is empty or contains no valid URLs");
    }

    return urls;
}



int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        cerr << "CURL ошибка инизиализации " << endl;
        return 1;
    }
    struct CurlGlobalCleanup {
        ~CurlGlobalCleanup() { curl_global_cleanup(); }
    } curl_cleanup;

    try {
        string url, directoryPath, threadCountStr;

        cout << "Введите URL файла:";

        getline(cin, url);
        cout << "Укажите путь для сохранения:";
        getline(cin, directoryPath);
        
        cout << "Укажите количество потоков 1-999:";
        getline(cin, threadCountStr);

        if (url.empty() || directoryPath.empty() || threadCountStr.empty()) {
            cerr << "Ошибка: нужно ввести все параметры" << endl;
            return 1;
        }

        if (!filesystem::exists(url)) {
            cerr << "Ошибка: url файла не сущетсвует " << url << endl;
            return 1;
        }

        int threadCount;
        try {
            threadCount = stoi(threadCountStr);
            if (threadCount < 1 || threadCount > 99) {
                cerr << "Ошибка количество потоков должно быть от 1 до 99" << endl;
                return 1;
            }
        }
        catch (...) {
            cerr << "Ошибка неправильный формат ввода" << endl;
            return 1;
        }

 
        vector<string> urls;
        try {
            urls = ReadUrlsFromFile(url);
            cout << "Чтение " << urls.size() << " URLs из файла" << endl;


            size_t preview_count = min(urls.size(), size_t(3));
            cout << "Первый " << preview_count << " URLs:" << endl;
            for (size_t i = 0; i < preview_count; ++i) {
                cout << "  " << (i + 1) << ". " << urls[i] << endl;
            }
            if (urls.size() > preview_count) {
                cout << "  ... and " << (urls.size() - preview_count) << " more" << endl;
            }
        }
        catch (const exception& e) {
            cerr << "Error reading URL file: " << e.what() << endl;
            return 1;
        }


        totalTasks = static_cast<int>(urls.size());
        for (size_t i = 0; i < urls.size(); ++i) {
            AddQueue(urls[i], directoryPath, static_cast<int>(i + 1));
        }

        cout << "\n=== Начало загрузки ===" << endl;
        cout << "URL файл: " << url << endl;
        cout << "Загрузка в директорию: " << directoryPath << endl;
        cout << "Потоки: " << threadCount << endl;
        cout << "Всего URLs: " << totalTasks << endl;
        cout << "========================\n" << endl;

        vector<thread> workers;
        for (int i = 0; i < threadCount; ++i) {
            workers.emplace_back(WorkerThread);
        }

        while (true) {
            this_thread::sleep_for(chrono::milliseconds(500));

            unique_lock<mutex> lock(qMutex);
            if (taskQueue.empty() && activeThreads == 0) {
                break;
            }

 
            static auto last_report = chrono::steady_clock::now();
            auto now = chrono::steady_clock::now();
            if (chrono::duration_cast<chrono::seconds>(now - last_report).count() >= 5) {
                int processed = completedTasks + failedTasks;
                cout << "[Status] " << processed << "/" << totalTasks << " ("
                    << (totalTasks > 0 ? (processed * 100 / totalTasks) : 0) << "%)" << endl;
                last_report = now;
            }
        }

        stopThreads = true;
        condition.notify_all();

        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        cout << "\n=== Загрузка завершена ===" << endl;
        cout << "Всего URLs: " << totalTasks << endl;
        cout << "Успешно: " << completedTasks << endl;
        cout << "Провалено: " << failedTasks << endl;
        cout << "Процент успеха: " << (totalTasks > 0 ? (completedTasks * 100 / totalTasks) : 0) << "%" << endl;

    }
    catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << endl;
        return 1;
    }

    return 0;
}


//int main() {
//    SetConsoleCP(1251);
//    SetConsoleOutputCP(1251);
//
//
//    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
//        cerr << "Ошибка иницилиазия CURL" << endl;
//        return 1;
//    }
//
//
//    string url, directoryPath, threadCountStr;
//    int threadCount = 0;
//
//    cout << "Введите URL файла:";
//    getline(cin, url);
//
//    cout << "Укажите путь для сохранения:";
//    getline(cin, directoryPath);
//
//    cout << "Укажите количество потоков 1-999:";
//    getline(cin, threadCountStr);
//
//
//    if (url.empty() || directoryPath.empty() || threadCountStr.empty()) {
//        cerr << "Ошибка: Параметры должные быть заполнены" << endl;
//        curl_global_cleanup();
//        return 1;
//    }
//    if (!filesystem::exists(url)) {
//        cerr << "Ошибка: '" << url << "'не существует" << endl;
//        curl_global_cleanup();
//        return 1;
//    }
//
//    try {
//        threadCount = stoi(threadCountStr);
//        if (threadCount < 1 || threadCount > 999) {
//            cerr << "Ошибка: количество загрузок должно бьть от 1 до 999" << endl;
//            curl_global_cleanup();
//            return 1;
//        }
//    }
//    catch (const exception& e) {
//        cerr << "Ошибка: неверный формат чисел" << endl;
//        curl_global_cleanup();
//        return 1;
//    }
//
//    vector<string> urls;
//
//    try {
//        urls = ReadUrlsFromFile(url);
//        cout << "Прочитано URL из файла: " << urls.size() << std::endl;
//
//    
//        cout << "Первые 3 URL:" << std::endl;
//        for (size_t i = 0; i < min(urls.size(), size_t(3)); ++i) {
//            std::cout << "  " << (i + 1) << ". " << urls[i] << std::endl;
//        }
//        if (urls.size() > 3) {
//            cout << "  ... и еще " << (urls.size() - 3) << " URL" << std::endl;
//        }
//    }
//    catch (const std::exception& e) {
//        cerr << "Ошибка чтения файла: " << e.what() << std::endl;
//        curl_global_cleanup();
//        return 1;
//    }
//
//    cout << "\n=== Начало загрузки ===" << endl;
//    cout << "Файл с URL: " << url << endl;
//    cout << "Директория для сохранения: " << directoryPath << endl;
//    cout << "Количество одновременных загрузок: " << threadCount << endl;
//    cout << "Всего URL для загрузки: " << urls.size() << endl;
//    cout << "========================\n" << endl;
//
//
//    totalTasks = static_cast<int>(urls.size());
//
//    for (size_t i = 0; i < urls.size(); ++i) {
//        AddQueue(urls[i], directoryPath, static_cast<int>(i + 1));
//    }
//
//    vector<thread> workers;
//    for (int i = 0; i < threadCount; ++i) {
//        workers.emplace_back(WorkerThread);
//    }
//
//    /*totalTasks = urls.size();
//    for (size_t i = 0; i < urls.size(); ++i){
//        AddQueue(urls[i], directoryPath, i + 1);
//     }
//    {
//        unique_lock<mutex> lock(queueMutex);
//        condition.wait(lock, [] {
//            return taskQueue.empty() && activeThreads == 0;
//            });
//    }*/
//    while (true) {
//        this_thread::sleep_for(chrono::milliseconds(100));
//        lock_guard<mutex> lock(qMutex);
//        if (taskQueue.empty() && activeThreads == 0) {
//            break;
//        }
//    }
//
//    stopThreads = true;
//    condition.notify_all();
//    for (auto& worker : workers) {
//        if (worker.joinable()) {
//            worker.join();
//        }
//    }
//
//        cout << "\n=== Статус загрузки ===" << endl;
//        cout << "Всего URL в файле: " << totalTasks << endl;
//        cout << "Успешно загружено:" << completedTasks << "файлов" << endl;
//        cout << "Не удалось загрузить:" << failedTasks << "файлов" << endl;
//        cout << "Всего задач:" << (totalTasks > 0? (completedTasks * 100 / totalTasks) : 0) << "%" << endl;
//    
//        curl_global_cleanup();
//
//        return 0;
//
//        // if (url.empty() || directoryPath.empty()) {
//        //     cerr << "URL и путь не должны быть пустыми" << endl;
//        //     return 1;
//        // }
//        /////* if (DowloadFunc(url, directoryPath)) {
//        ////      cout << "Файл успешно скачан!" << std:endl;
//        //// }
//        //// else {
//        ////     std::cerr << "ошибка скачивания файла" << endl;
//        ////     curl_global_cleanup();
//        ////     return 1;
//        //// }*/
//        //// curl_global_cleanup();
//        //return 0;
//}
   
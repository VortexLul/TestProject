#include <iostream>
#include <string>
#include <curl/curl.h>
#include <fstream>

using namespace std;

size_t writeData(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}
int main() {

    string url = "https://bolid.ru/bld/images/logo.png";
    string filename = "downloaded_image.png";

    CURL* curl = curl_easy_init();
    if (curl) {
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filename.c_str(), "wb");

        if (err != 0 || fp == nullptr) {
            cout << "Не удалось открыть файл для записи" << endl;
            curl_easy_cleanup(curl);
            return 1;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);


        CURLcode res = curl_easy_perform(curl);
       

        fclose(fp);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            cout << "Неудалось загрузить изображение" << curl_easy_strerror(res) << endl;
            remove(filename.c_str());
        }
        else {
            cout << "Загрузка произошла успешно" << filename << endl;
        }
    }
    return 0;

}
   
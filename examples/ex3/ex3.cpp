#if 0
#include <filesystem>
#include <chrono>
#include <memory>
#include <fstream>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/create.hpp>

#include <curl/curl.h>

namespace ex = stdexec;
namespace fs = std::filesystem;

using namespace std::chrono_literals;

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
static int progress_callback(void *userdata,
                             curl_off_t /*dltotal*/,
                             curl_off_t /*dlnow*/,
                             curl_off_t /*ultotal*/,
                             curl_off_t /*ulnow*/);

struct CurlResponse
{
  // Даже при успешном запросе сам сервер может вернуть ошибку, например,
  // недостаточно прав доступа для скачивания файла.
  int http_code;

  // Путь, где хранится скачанный файл
  fs::path file_path;

  // Размер скачанного файла
  double download_size;
};

class CurlDownloadSender
{
  public:
    // Определяем возможные варианты завершения операции
    using completion_signatures = ex::completion_signatures<
      // Успех — возвращаем информацию о загрузке
      ex::set_value_t(CurlResponse),
      // Ошибка — передаём исключение
      ex::set_error_t(std::exception_ptr),
      // Отмена — операция прервана
      ex::set_stopped_t()
    >;

    CurlDownloadSender(
      std::string url,
      fs::path output_path,
      std::chrono::seconds timeout = std::chrono::seconds(30)
    ) : url_(std::move(url)),
        output_path_(std::move(output_path)),
        timeout_(timeout) {}

    template <class Receiver>
    struct OperationState
    {
      Receiver receiver_;                  // Куда отправлять результат
      std::string url_;                    // Откуда скачивать
      fs::path output_path_;               // Куда сохранять
      std::chrono::seconds timeout_;       // Максимальное время загрузки
      std::ofstream output_file_;          // Файл для записи
      ex::inplace_stop_token stop_token_;  // Токен отмены

      void start() noexcept
      {
        // Проверяем, не запрошена ли отмена ещё до начала работы
        if (stop_token_.stop_requested())
        {
          ex::set_stopped(std::move(receiver_));
          return;
        }

        // Открываем файл для записи
        output_file_.open(output_path_, std::ios::binary);
        if (!output_file_.is_open())
        {
          ex::set_error(
            std::move(receiver_),
            std::make_exception_ptr(std::runtime_error{"io_error"})
          );
          return;
        }

        // Инициализируем CURL
        using curl_ptr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
        curl_ptr curl_handle{curl_easy_init(), &curl_easy_cleanup};
        if (!curl_handle)
        {
          ex::set_error(
            std::move(receiver_),
            std::make_exception_ptr(std::runtime_error{"Failed to init curl"})
          );
          return;
        }

        // Настраиваем CURL
        curl_easy_setopt(curl_handle.get(), CURLOPT_URL, url_.c_str());
        curl_easy_setopt(curl_handle.get(), CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_handle.get(), CURLOPT_WRITEDATA, this);
        //curl_easy_setopt(curl_handle.get(), CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl_handle.get(), CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl_handle.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_handle.get(), CURLOPT_TIMEOUT, timeout_.count());

        // Выполняем запрос
        CURLcode res = curl_easy_perform(curl_handle.get());
        output_file_.close();

        // Проверяем, была ли операция отменена через progress_callback
        if (res == CURLE_ABORTED_BY_CALLBACK)
        {
          fs::remove(output_path_);
          ex::set_stopped(std::move(receiver_));
          return;
        }

        // Проверяем другие ошибки CURL
        if (res != CURLE_OK)
        {
          fs::remove(output_path_);
          ex::set_error(
            std::move(receiver_),
            std::make_exception_ptr(
              std::runtime_error
              {
                std::format("Curl error: {}", curl_easy_strerror(res))
              }
            )
          );
          return;
        }

        // Получаем информацию о скачивании
        long http_code = 0;
        double download_size = 0.0;

        curl_easy_getinfo(curl_handle.get(),
                          CURLINFO_RESPONSE_CODE,
                          &http_code);
        curl_easy_getinfo(curl_handle.get(),
                          CURLINFO_SIZE_DOWNLOAD_T,
                          &download_size);

        // Проверяем HTTP-код
        if (http_code >= 400)
        {
          fs::remove(output_path_);
          ex::set_error(
            std::move(receiver_),
            std::make_exception_ptr(
              std::runtime_error
              {
                std::format("invalid response: http_code={}", http_code)
              }
            )
          );
          return;
        }

        // Всё прошло успешно — формируем результат
        CurlResponse response
        {
          .http_code = static_cast<int>(http_code),
          .file_path = output_path_,
          .download_size = download_size,
        };

        ex::set_value(std::move(receiver_), std::move(response));
      }
    };

    template <class Receiver>
    auto connect(Receiver r) const
    {
      // Используем ресивер для получения контекста
      auto env = ex::get_env(r);
      // Получаем текущий токен остановки из контекста
      auto token = ex::get_stop_token(env);

      return OperationState<Receiver>
      {
        std::move(r),
        url_,
        output_path_,
        timeout_,
        std::ofstream{},
        std::move(token)
      };
    }

  private:
    std::string url_;
    fs::path output_path_;
    std::chrono::seconds timeout_;
};

// Callback-функция для записи данных в файл
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  auto *out = static_cast<std::ofstream *>(userdata);
  out->write(ptr, size * nmemb);
  return size * nmemb;
}

/*
static int progress_callback(void *userdata,
                             curl_off_t,
                             curl_off_t,
                             curl_off_t,
                             curl_off_t)
{
  auto *op = static_cast<OperationState *>(userdata);

  // 0 = продолжаем загрузку, любое другое значение = прерываем
  return op->stop_token_.stop_requested() ? 1 : 0;
}
*/

int main()
{
  exec::static_thread_pool pool{3};
  auto sched = pool.get_scheduler();

  auto pipeline =
    ex::when_all(
      ex::on(sched,
             CurlDownloadSender
             {
               "https://example.com/file.sig",
               "downloads/file.sig",
               30s
             }// | ex::then(verify_signature)
      ),
      ex::on(sched,
             CurlDownloadSender
             {
               "https://example.com/file.sha256",
               "downloads/file.sha256",
               30s
             } //| ex::then(parse_checksum)
      ),
      ex::on(sched,
             CurlDownloadSender
             {
               "https://example.com/file.dat",
               "downloads/file.dat",
               30s
             }
      )
    ); // | ex::then(compare_checksum);

  return 0;
}
#endif

int main()
{
  return 0;
}

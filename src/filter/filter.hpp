#pragma once

#include <set>
#include <string>
#include <string_view>

/**
 * @brief Фильтр URL на основе списка заблокированных базовых адресов.
 */
class Filter
{
public:
    /**
     * @brief Создаёт фильтр и загружает правила из указанного JSON-файла.
     *
     * @param configPath Путь к JSON-файлу с правилами фильтрации.
     */
    explicit Filter(std::string_view configPath);

    /**
     * @brief Проверяет, заблокирован ли URL.
     *
     * URL считается заблокированным, если он совпадает с одним из базовых
     * адресов или находится внутри него.
     *
     * @param url Проверяемый URL.
     */
    bool isBlocked(std::string_view url) const;

    /**
     * @brief Добавляет базовый URL в список блокировки.
     *
     * @param url Базовый URL для блокировки.
     */
    void addUrl(std::string_view url);

    /**
     * @brief Удаляет базовый URL из списка блокировки.
     *
     * @param url Базовый URL для удаления.
     */
    void removeUrl(std::string_view url);

private:
    /**
     * @brief Проверяет, соответствует ли URL указанному базовому адресу.
     *
     * Совпадение учитывает границу базового URL, чтобы не блокировать похожие
     * домены с другим именем.
     *
     * @param url Проверяемый URL.
     * @param baseUrl Базовый URL из списка блокировки.
     */
    static bool matchesBaseUrl(std::string_view url, std::string_view baseUrl);

    /**
     * @brief Нормализует базовый URL перед сохранением в список блокировки.
     *
     * @param url URL для нормализации.
     */
    static std::string normalizeBaseUrl(std::string_view url);

    /**
     * @brief Загружает правила фильтрации из JSON-файла.
     *
     * @param configPath Путь к JSON-файлу с правилами.
     */
    void loadFromFile(std::string_view configPath);

    std::set<std::string> blacklist;
};

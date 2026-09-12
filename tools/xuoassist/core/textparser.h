// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/textparser.h): in-memory token parser for
// cfg-style text. QString -> std::string, QStringList -> std::vector<std::string>.

#ifndef XUOASSIST_TEXTPARSER_H
#define XUOASSIST_TEXTPARSER_H

#include "global.h"
#include "../model/datareader.h"

namespace xuoassist {

class CTextParser
{
    SETGET(string, RawLine, "")
    SETGET(bool, SaveQuotes, false)

private:
    // Загруженный текст
    string m_Text;

    // Указатель на текущий обрабатываемый символ
    uchar *m_Ptr;

    // Указатель на начало предыдущей обрабатываемой строки
    uchar *m_StartPtr;

    // Адрес конца строки
    uchar *m_EOL;

    // Адрес конца файла
    uchar *m_End;

    CDataReader m_File;

    // Разделители
    char m_Delimiters[50];

    // Количество разделителей
    int m_DelimitersSize;

    // Комментарии
    char m_Comentaries[50];

    // Количество коментариев
    int m_ComentariesSize;

    // Набор кавычек, формат кавычек в наборе: openQuote, closeQuote (могут быть одинаковыми)
    char m_Quotes[50];

    // Количество элементов в наборе кавычек
    int m_QuotesSize;

    // Обрезать строки на выходе
    bool m_Trim{ true };

    // Получить конец строки
    void GetEOL();

    // Проверка на разделитель
    bool IsDelimiter();

    // Пропустить все до данных
    void SkipToData();

    // Проверка на комментарий
    bool IsComment();

    // Проверка на кавычку
    bool IsQuote();

    // Проверка на закрывающую кавычку
    bool IsSecondQuote();

    // Получить следующий токен
    string ObtainData();

    // Получить следующий фрагмент строки или токен (если кавычки не обнаружены)
    string ObtainQuotedData();

    void SaveRawLine();

public:
    CTextParser(
        const string &text,
        const char *delimiters = "",
        const char *comentaries = "",
        const char *quotes = "",
        const bool &saveQuotes = false);
    ~CTextParser();

    // Сбросить указатель на старт
    void Restart();

    // Проверка на конец файла
    bool IsEOF();

    void SavePosition();

    void LoadPosition();

    // Прочитать токены из файла
    vector<string> ReadTokens(bool trim = true);

    // Прочитать токены из строки
    vector<string> GetTokens(const char *str, bool trim = true);
};

} // namespace xuoassist

#endif // XUOASSIST_TEXTPARSER_H

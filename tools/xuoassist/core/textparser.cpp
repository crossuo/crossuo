// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/textparser.cpp), token-for-token identical.

#include "textparser.h"
#include <cstring>
#include <cstdlib>

namespace xuoassist {

CTextParser::CTextParser(
    const string &text, const char *delimiters, const char *comentaries, const char *quotes,
    const bool &saveQuotes)
    : m_RawLine("")
    , m_SaveQuotes(saveQuotes)
    , m_Text(text)
    , m_Ptr(nullptr)
    , m_StartPtr(nullptr)
    , m_EOL(nullptr)
    , m_File((uchar *)m_Text.c_str(), m_Text.length())
{
    // Инициализация разделителей
    memset(&m_Delimiters[0], 0, sizeof(m_Delimiters));
    m_DelimitersSize = strlen(delimiters);

    if (m_DelimitersSize != 0)
    {
        memcpy(&m_Delimiters[0], &delimiters[0], m_DelimitersSize);
    }

    // Инициализация комментариев
    memset(&m_Comentaries[0], 0, sizeof(m_Comentaries));
    m_ComentariesSize = strlen(comentaries);

    if (m_ComentariesSize != 0)
    {
        memcpy(&m_Comentaries[0], &comentaries[0], m_ComentariesSize);
    }

    // Инициализация кавычек
    memset(&m_Quotes[0], 0, sizeof(m_Quotes));
    m_QuotesSize = strlen(quotes);

    if (m_QuotesSize != 0)
    {
        memcpy(&m_Quotes[0], &quotes[0], m_QuotesSize);
    }

    // Вычисляем конец файла
    m_End = m_File.GetEnd();
}

CTextParser::~CTextParser()
{
}

// Сбросить указатель на старт
void CTextParser::Restart()
{
    m_File.ResetPtr();
}

// Проверка на конец файла
bool CTextParser::IsEOF()
{
    return (m_File.GetPtr() >= m_End);
}

// Получить конец строки
void CTextParser::GetEOL()
{
    // Конец строки равен текущему адресу
    m_EOL = m_File.GetPtr();

    // Если файл прочитан не до конца
    if (!IsEOF())
    {
        // Ищем конец строки
        while (m_EOL < m_End && (*m_EOL != 0u))
        {
            if (*m_EOL == '\n')
            {
                break;
            }

            m_EOL++;
        }
    }
}

// Проверка на разделитель
bool CTextParser::IsDelimiter()
{
    bool result = false;

    // Проход по всем разделителям
    for (int i = 0; i < m_DelimitersSize && !result; i++)
    {
        result = (*m_Ptr == m_Delimiters[i]);
    }

    return result;
}

// Пропустить все до данных
void CTextParser::SkipToData()
{
    // Если символ - разделитель, то проход по всем разделителям и смещение указателя
    while (m_Ptr < m_EOL && (*m_Ptr != 0u) && IsDelimiter())
    {
        m_Ptr++;
    }
}

// Проверка на комментарий
bool CTextParser::IsComment()
{
    bool result = (*m_Ptr == '\n');

    // Проход по всем комментариям
    for (int i = 0; i < m_ComentariesSize && !result; i++)
    {
        result = (*m_Ptr == m_Comentaries[i]);

        if (result && i + 1 < m_ComentariesSize && m_Comentaries[i] == m_Comentaries[i + 1] &&
            m_Ptr + 1 < m_EOL)
        {
            result = (m_Ptr[0] == m_Ptr[1]);
            i++;
        }
    }

    return result;
}

// Проверка на кавычку
bool CTextParser::IsQuote()
{
    bool result = (*m_Ptr == '\n');

    // Пройдемся по кавычкам, формат кавычек в наборе: openQuote, closeQuote (могут быть одинаковыми)
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        // Если кавычка нашлась
        if (*m_Ptr == m_Quotes[i] || *m_Ptr == m_Quotes[i + 1])
        {
            result = true;
            break;
        }
    }

    return result;
}

// Проверка на закрывающую кавычку
bool CTextParser::IsSecondQuote()
{
    bool result = (*m_Ptr == '\n');

    // Пройдемся по кавычкам, формат кавычек в наборе: openQuote, closeQuote (могут быть одинаковыми)
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        // Если кавычка нашлась
        if (*m_Ptr == m_Quotes[i + 1])
        {
            result = true;
            break;
        }
    }

    return result;
}

// Получить следующий токен
string CTextParser::ObtainData()
{
    string result = "";

    // Пока разделитель валиден - записываем данные и смещаем указатель
    while (m_Ptr < m_End && (*m_Ptr != 0u) && *m_Ptr != '\n')
    {
        // Проверка на разделитель
        if (IsDelimiter() || IsQuote())
        {
            break;
            // Проверка на комментарий
        }
        if (IsComment())
        {
            m_Ptr = m_EOL;
            break;
        }

        // Проверяем на перенос строки, при необходимости обрезаем пробелы
        if (*m_Ptr != '\r' && (!m_Trim || (*m_Ptr != ' ' && *m_Ptr != '\t')))
        {
            result += *m_Ptr;
        }

        m_Ptr++;
    }

    return result;
}

// Получить следующий фрагмент строки или токен (если кавычки не обнаружены)
string CTextParser::ObtainQuotedData()
{
    bool exit = false;
    string result;

    // Пройдемся по кавычкам, формат кавычек в наборе: openQuote, closeQuote (могут быть одинаковыми)
    for (int i = 0; i < m_QuotesSize; i += 2)
    {
        // Если кавычка нашлась
        if (*m_Ptr == m_Quotes[i])
        {
            // Запомним закрывающую кавычку
            char endQuote = m_Quotes[i + 1];
            exit = true;

            // Смещаем указатель, т.к. кавычку писать не нужно
            m_Ptr++;
            uchar *ptr = m_Ptr;

            // Пропустим все до конца строки или до закрывающей кавычки
            while (ptr < m_EOL && (*ptr != 0u) && *ptr != '\n' && *ptr != endQuote)
            {
                ptr++;
            }

            // Размер фрагмента
            int size = ptr - m_Ptr;

            if (size > 0)
            {
                // Выделяем память под фрагмент
                result.resize(size + 1);

                // Копируем фрагмент
                memcpy(&result[0], &m_Ptr[0], size);
                result[size] = 0;

                // Пройдемся по фрагменту с конца и затрем лишнее
                for (int j = size - 1; j >= 0 && result[j] == '\r'; j--)
                {
                    result[j] = 0;
                }

                // Указатель на конец фрагмена
                m_Ptr = ptr;

                if (m_Ptr < m_EOL && *m_Ptr == endQuote)
                {
                    m_Ptr++;
                }
            }

            if (m_SaveQuotes)
            {
                result.insert(result.begin(), m_Quotes[i]);
                result = string(result) + endQuote;
            }

            break;
        }
    }

    // Если это не кавычки - вычленим слово
    if (!exit)
    {
        return ObtainData();
    }

    return result;
}

// Прочитать токены из файла
void CTextParser::SaveRawLine()
{
    int size = m_EOL - m_Ptr;

    if (size > 0)
    {
        m_RawLine.resize(size, 0);
        memcpy(&m_RawLine[0], &m_Ptr[0], size);

        while ((m_RawLine.length() != 0u) &&
               (m_RawLine[size - 1] == '\r' || m_RawLine[size - 1] == '\n'))
        {
            m_RawLine.resize(m_RawLine.length() - 1);
        }
    }
    else
    {
        m_RawLine = "";
    }
}

void CTextParser::SavePosition()
{
    m_StartPtr = m_File.GetPtr();
}

void CTextParser::LoadPosition()
{
    m_File.SetPtr(m_StartPtr);
}

// Прочитать токены из файла
vector<string> CTextParser::ReadTokens(bool trim)
{
    m_Trim = trim;
    vector<string> result;

    // Если не достигли конца файла
    if (!IsEOF())
    {
        // Инициализация указателей
        m_Ptr = m_File.GetPtr();
        GetEOL();

        SaveRawLine();

        // Проход до конца строки
        while (m_Ptr < m_EOL)
        {
            // Пропустить разделители
            SkipToData();

            // Если это комментарий - выходим
            if (IsComment())
            {
                break;
            }

            // Получаем токен
            string buf = ObtainQuotedData();

            // Если токен не пуст - запишем его в стек
            if (buf.length() != 0)
            {
                result.push_back(buf);
            }
            else if (IsSecondQuote())
            {
                m_Ptr++;
            }
        }

        m_File.SetPtr(m_EOL + 1);
    }

    return result;
}

// Прочитать токены из строки
vector<string> CTextParser::GetTokens(const char *str, bool trim)
{
    m_Trim = trim;
    vector<string> result;

    // Сохраним предыдущее значения конца данных
    uchar *oldEnd = m_End;

    m_Ptr = (uchar *)str;
    m_End = (uchar *)str + strlen(str);
    m_EOL = m_End;

    SaveRawLine();

    // Пройдемся по строке
    while (m_Ptr < m_EOL)
    {
        // Пропустить разделители
        SkipToData();

        // Если это комментарий - выходим
        if (IsComment())
        {
            break;
        }

        // Получаем токен
        string buf = ObtainQuotedData();

        // Если токен не пуст - запишем его в стек
        if (buf.length() != 0)
        {
            result.push_back(buf);
        }
        else if (IsSecondQuote())
        {
            m_Ptr++;
        }
    }

    // Восстановим предыдущее значения конца данных
    m_End = oldEnd;

    return result;
}

} // namespace xuoassist

#include "lzstring.h"

#include <cmath>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringBuilder>
#include <QStringRef>
#include <QVariant>

QString LZString::keyStrBase64 = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=");
QString LZString::keyStrUriSafe = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-$");

static QChar compressGetCharFromInt(int a)
{
    return QChar(a);
}

static  QChar compressToUTF16GetCharFromInt(int a)
{
    return QChar(a+32);
}

static QChar compressToBase64GetCharFromInt(int a)
{
    return LZString::keyStrBase64[a];
}


QString LZString::compress(const QString &uncompressed)
{
    return _compress(uncompressed, 16, &compressGetCharFromInt);
}

QString LZString::compressToUTF16(const QString &uncompressed)
{
    if (uncompressed.isEmpty())
        return "";

    return _compress(uncompressed, 15, &compressToUTF16GetCharFromInt) % " ";  // % - QStringBuilder
}

QString LZString::compressToBase64(const QString &uncompressed)
{
    if (uncompressed.isEmpty())
        return "";

    QString res = _compress(uncompressed, 6, &compressToBase64GetCharFromInt);  // % - QStringBuilder
    switch (res.length() % 4) { // To produce valid Base64
        default: // When could this happen ?
        case 0 : return res;
        case 1 : return res.append("===");
        case 2 : return res.append("==");
        case 3 : return res.append("=");
    }
    return res;
}


template <typename GetCharFromInt>
QString LZString::_compress(const QString &uncompressed, int bitsPerChar, GetCharFromInt getCharFromInt)
{
    int i = 0;
    int value = 0;
    QHash<QStringRef, int> context_dictionary;
    QHash<QStringRef, bool> context_dictionaryToCreate;
    QStringRef context_c;
    QStringRef context_wc;
    QStringRef context_w;
    int context_enlargeIn = 2; // Compensate for the first entry which should not count
    int context_dictSize = 3;
    int context_numBits = 2;
    QString context_data;
    int context_data_val = 0;
    int context_data_position = 0;

    context_data.reserve( qMax(255, uncompressed.length()/5) );
    context_w = QStringRef(&uncompressed, 0, 0);

    for (int ii=0, size=uncompressed.length(); ii<size; ++ii)
    {
        context_c = QStringRef(&uncompressed, ii, 1);
        if (!context_dictionary.contains(context_c))
        {
            context_dictionary.insert(context_c, context_dictSize++);
            context_dictionaryToCreate.insert(context_c, true);
        }

        context_wc = QStringRef(&uncompressed, context_w.position(), context_w.length()+1);
        if (context_dictionary.contains(context_wc))
        {
            context_w = context_wc;
        }
        else
        {
            if (context_dictionaryToCreate.contains(context_w))
            {
                if (context_w.at(0).unicode() < 256)
                {
                    for (i=0; i<context_numBits; ++i)
                    {
                        context_data_val = (context_data_val << 1);
                        if (context_data_position == bitsPerChar-1)
                        {
                            context_data_position = 0;
                            context_data.append(getCharFromInt(context_data_val));
                            context_data_val = 0;
                        }
                        else
                        {
                            context_data_position++;
                        }
                    }
                    value = context_w.at(0).unicode();
                    for (i=0; i<8; ++i)
                    {
                        context_data_val = (context_data_val << 1) | (value & 1);
                        if (context_data_position == bitsPerChar-1)
                        {
                            context_data_position = 0;
                            context_data.append(getCharFromInt(context_data_val));
                            context_data_val = 0;
                        }
                        else
                        {
                            context_data_position++;
                        }
                        value = value >> 1;
                    }
                }
                else
                {
                    value = 1;
                    for (i=0; i<context_numBits; ++i)
                    {
                        context_data_val = (context_data_val << 1) | value;
                        if (context_data_position == bitsPerChar-1)
                        {
                            context_data_position = 0;
                            context_data.append(getCharFromInt(context_data_val));
                            context_data_val = 0;
                        }
                        else
                        {
                            context_data_position++;
                        }
                        value = 0;
                    }
                    value = context_w.at(0).unicode();
                    for (i=0; i<16; ++i)
                    {
                        context_data_val = (context_data_val << 1) | (value & 1);
                        if (context_data_position == bitsPerChar-1)
                        {
                            context_data_position = 0;
                            context_data.append(getCharFromInt(context_data_val));
                            context_data_val = 0;
                        }
                        else
                        {
                            context_data_position++;
                        }
                        value = value >> 1;
                    }
                }
                context_enlargeIn--;
                if (context_enlargeIn == 0)
                {
                    context_enlargeIn = std::pow(2.0, context_numBits);
                    context_numBits++;
                }
                context_dictionaryToCreate.remove(context_w);
            }
            else
            {
                value = context_dictionary.value(context_w);
                for (i=0; i<context_numBits; ++i)
                {
                    context_data_val = (context_data_val << 1) | (value & 1);
                    if (context_data_position == bitsPerChar-1)
                    {
                        context_data_position = 0;
                        context_data.append(getCharFromInt(context_data_val));
                        context_data_val = 0;
                    }
                    else
                    {
                        context_data_position++;
                    }
                    value = value >> 1;
                }
            }
            context_enlargeIn--;
            if (context_enlargeIn == 0)
            {
                context_enlargeIn = std::pow(2.0, context_numBits);
                context_numBits++;
            }

            // Add wc to the dictionary.
            context_dictionary.insert(context_wc, context_dictSize++);
            context_w = context_c;
        }
    }

    // Output the code for w.
    if (!context_w.isEmpty())
    {
        if (context_dictionaryToCreate.contains(context_w))
        {
            if (context_w.at(0).unicode() < 256)
            {
                for (i=0; i<context_numBits; ++i)
                {
                    context_data_val = (context_data_val << 1);
                    if (context_data_position == bitsPerChar-1)
                    {
                        context_data_position = 0;
                        context_data.append(getCharFromInt(context_data_val));
                        context_data_val = 0;
                    }
                    else
                    {
                        context_data_position++;
                    }
                }
                value = context_w.at(0).unicode();
                for (i=0; i<8; ++i)
                {
                    context_data_val = (context_data_val << 1) | (value & 1);
                    if (context_data_position == bitsPerChar-1)
                    {
                        context_data_position = 0;
                        context_data.append(getCharFromInt(context_data_val));
                        context_data_val = 0;
                    }
                    else
                    {
                        context_data_position++;
                    }
                    value = value >> 1;
                }
            }
            else
            {
                value = 1;
                for (i=0; i<context_numBits; ++i)
                {
                    context_data_val = (context_data_val << 1) | value;
                    if (context_data_position == bitsPerChar-1)
                    {
                        context_data_position = 0;
                        context_data.append(getCharFromInt(context_data_val));
                        context_data_val = 0;
                    }
                    else
                    {
                        context_data_position++;
                    }
                    value = 0;
                }
                value = context_w.at(0).unicode();
                for (i=0; i<16; ++i)
                {
                    context_data_val = (context_data_val << 1) | (value & 1);
                    if (context_data_position == bitsPerChar-1)
                    {
                        context_data_position = 0;
                        context_data.append(getCharFromInt(context_data_val));
                        context_data_val = 0;
                    }
                    else
                    {
                        context_data_position++;
                    }
                    value = value >> 1;
                }
            }
            context_enlargeIn--;
            if (context_enlargeIn == 0)
            {
                context_enlargeIn = std::pow(2.0, context_numBits);
                context_numBits++;
            }
            context_dictionaryToCreate.remove(context_w);
        }
        else
        {
            value = context_dictionary.value(context_w);
            for (i=0; i<context_numBits; ++i)
            {
                context_data_val = (context_data_val << 1) | (value & 1);
                if (context_data_position == bitsPerChar-1)
                {
                    context_data_position = 0;
                    context_data.append(getCharFromInt(context_data_val));
                    context_data_val = 0;
                }
                else
                {
                    context_data_position++;
                }
                value = value >> 1;
            }
        }

        context_enlargeIn--;
        if (context_enlargeIn == 0)
        {
            context_enlargeIn = std::pow(2.0, context_numBits);
            context_numBits++;
        }
    }

    // Mark the end of the stream
    value = 2;
    for (i=0; i<context_numBits; ++i)
    {
        context_data_val = (context_data_val << 1) | (value & 1);
        if (context_data_position == bitsPerChar-1)
        {
            context_data_position = 0;
            context_data.append(getCharFromInt(context_data_val));
            context_data_val = 0;
        }
        else
        {
            context_data_position++;
        }
        value = value >> 1;
    }

    // Flush the last char
    while (true)
    {
        context_data_val = (context_data_val << 1);
        if (context_data_position == bitsPerChar-1)
        {
            context_data.append(getCharFromInt(context_data_val));
            break;
        }
        else
        {
            context_data_position++;
        }
    }

    return context_data;
}

class DecompressGetNextValue
{
public:
    DecompressGetNextValue(const QString &compressed) : m_compressed(compressed.constData()) {}
    int operator()(int index) const
    {
        return m_compressed[index].unicode();
    }

private:
    const QChar *m_compressed;
};

class DecompressFromUTF16GetNextValue
{
public:
    DecompressFromUTF16GetNextValue(const QString &compressed) :
        m_compressed(compressed.constData()) {}

    int operator()(int index) const
    {
        return m_compressed[index].unicode() - 32;
    }

private:
    const QChar *m_compressed;
};

class DecompressGetBaseValue
{
public:
    DecompressGetBaseValue(const QString &alphabet, const QString &compressed) : m_compressed(compressed.constData()), alphabet(alphabet)
    {
        int i=0;
        for(i=0; i<alphabet.length(); i++) {
            baseReverseDic[alphabet[i]] = i;
        }
    }

    int operator()(int index) const
    {
        return baseReverseDic[m_compressed[index].unicode()];
    }

private:
    const QChar *m_compressed;
    QString alphabet;
    QMap<QChar, int> baseReverseDic;
};

QString LZString::decompress(const QString &compressed)
{
    if (compressed.isEmpty())
        return "";

    return _decompress(compressed.length(), 32768, DecompressGetNextValue(compressed));
}

QString LZString::decompressFromUTF16(const QString &compressed)
{
    if (compressed.isEmpty())
        return "";

    return _decompress(compressed.length(), 16384, DecompressFromUTF16GetNextValue(compressed));
}

QString LZString::decompressFromBase64(const QString &compressed)
{
    if (compressed.isEmpty())
        return "";

    return _decompress(compressed.length(), 32, DecompressGetBaseValue(LZString::keyStrBase64, compressed));
}

struct DecompressData
{
    int val;
    int position;
    int index;
};

template <typename GetNextValue>
QString LZString::_decompress(int length, int resetValue, GetNextValue getNextValue)
{
    QList<QString> dictionary;
    int next = 0;
    int enlargeIn = 4;
    int dictSize = 4;
    int numBits = 3;
    QString entry;
    QString result;
    QString w;
    int bits, resb, maxpower, power;
    QString c;
    DecompressData data;
    data.val = getNextValue(0);
    data.position = resetValue;
    data.index = 1;

    result.reserve(length*5);

    // Put anything at dictionary indexes 0-2 - this will never be read.
    // See question:
    // http://pieroxy.net/blog/replyToBlogEntry.action?entry=1368091620000&comment=1474381950353
    // and response:
    // http://pieroxy.net/blog/replyToBlogEntry.action?entry=1368091620000&comment=1474435834165
    for (int i=0; i<3; ++i)
    {
        dictionary.append(QString::number(i));
    }

    bits = 0;
    maxpower = std::pow(2.0, 2);
    power = 1;
    while (power != maxpower)
    {
        resb = data.val & data.position;
        data.position >>= 1;
        if (data.position == 0)
        {
            data.position = resetValue;
            data.val = getNextValue(data.index++);
        }
        bits |= (resb>0 ? 1 : 0) * power;
        power <<= 1;
    }

    switch (next = bits)
    {
        case 0:
            bits = 0;
            maxpower = std::pow(2.0, 8);
            power = 1;
            while (power != maxpower)
            {
                resb = data.val & data.position;
                data.position >>= 1;
                if (data.position == 0)
                {
                    data.position = resetValue;
                    data.val = getNextValue(data.index++);
                }
                bits |= (resb>0 ? 1 : 0) * power;
                power <<= 1;
            }
            c = QChar(bits);
            break;

        case 1:
            bits = 0;
            maxpower = std::pow(2.0, 16);
            power = 1;
            while (power != maxpower)
            {
                resb = data.val & data.position;
                data.position >>= 1;
                if (data.position == 0)
                {
                    data.position = resetValue;
                    data.val = getNextValue(data.index++);
                }
                bits |= (resb>0 ? 1 : 0) * power;
                power <<= 1;
            }
            c = QChar(bits);
            break;

        case 2:
            return "";
    }

    dictionary.append(c);  // "c" may be empty string
    w = c;
    result.append(c);
    while (true)
    {
        if (data.index > length)
            return "";

        bits = 0;
        maxpower = std::pow(2.0, numBits);
        power = 1;
        while (power != maxpower)
        {
            resb = data.val & data.position;
            data.position >>= 1;
            if (data.position == 0)
            {
                data.position = resetValue;
                data.val = getNextValue(data.index++);
            }
            bits |= (resb>0 ? 1 : 0) * power;
            power <<= 1;
        }

        // Very strange here, "c" above is as char/string, but
        // further "c" is a int, rename "c" in the switch as "cc".
        int cc;
        switch (cc = bits)
        {
            case 0:
                bits = 0;
                maxpower = std::pow(2.0, 8);
                power = 1;
                while (power != maxpower)
                {
                    resb = data.val & data.position;
                    data.position >>= 1;
                    if (data.position == 0)
                    {
                        data.position = resetValue;
                        data.val = getNextValue(data.index++);
                    }
                    bits |= (resb>0 ? 1 : 0) * power;
                    power <<= 1;
                }

                cc = dictSize++;
                dictionary.append(QChar(bits));
                enlargeIn--;
                break;

            case 1:
                bits = 0;
                maxpower = std::pow(2.0, 16);
                power = 1;
                while (power != maxpower)
                {
                    resb = data.val & data.position;
                    data.position >>= 1;
                    if (data.position == 0)
                    {
                        data.position = resetValue;
                        data.val = getNextValue(data.index++);
                    }
                    bits |= (resb>0 ? 1 : 0) * power;
                    power <<= 1;
                }

                cc = dictSize++;
                dictionary.append(QChar(bits));
                enlargeIn--;
                break;

            case 2:
                return result;
        }

        if (enlargeIn == 0)
        {
            enlargeIn = std::pow(2.0, numBits);
            numBits++;
        }

        Q_ASSERT(dictSize == dictionary.size());
        if (cc < dictSize && !dictionary.at(cc).isEmpty())
        {
            entry = dictionary.at(cc);
        }
        else
        {
            if (cc == dictSize)
                entry = w % w.at(0);  // % - QStringBuilder
            else
                return QString();
        }
        result.append(entry);

        // Add w+entry[0] to the dictionary.
        dictSize++;
        dictionary.append(w % entry.at(0));  // % - QStringBuilder
        enlargeIn--;

        w = entry;

        if (enlargeIn == 0)
        {
            enlargeIn = std::pow(2.0, numBits);
            numBits++;
        }
    }
}

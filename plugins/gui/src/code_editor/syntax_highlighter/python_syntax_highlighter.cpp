#include "gui/code_editor/syntax_highlighter/python_syntax_highlighter.h"

#include "gui/code_editor/syntax_highlighter/python_qss_adapter.h"
namespace hal
{
    PythonSyntaxHighlighter::PythonSyntaxHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent), mStringPrefixes({"r", "u", "b", "f", "br", "rb", "fr", "rf"})
    //      //mKeywordColor(177, 240, 0),
    //      //mKeywordColor("#ffd200"),
    //      mKeywordColor("#CC7832"),
    //      //mKeywordColor("#6419E6"),
    //      //mKeywordColor("#9019E6"),
    //      //mOperatorColor("#0A81FF"),
    //      mOperatorColor("#A9B7C6"), mBraceColor("#A9B7C6"), mDefclassColor("#A9B7C6"),
    //      //mSingleQuotedStringColor("#17de4c"),
    //      //mDoubleQuotedStringColor("#17de4c"),
    //      mSingleQuotedStringColor("#A5C25C"), mDoubleQuotedStringColor("#A5C25C"),
    //      mSelfColor("#A9B7C6"),
    //      //mNumberColor(236, 118, 0),
    //      //mNumberColor("#DB8000"),
    //      //mNumberColor("#89F406"),
    //      //mNumberColor("#F4EC06"),
    //      mNumberColor("#6897BB"),
    //      //mCommentColor("#6F7B7B")
    //      //mCommentColor(222, 233, 175)
    //      //mCommentColor("#556062")
    //      //mCommentColor("#30474B") //2.0
    //      //mCommentColor("#3A565A") //2.5
    //      mCommentColor("#629755")    //2.98
    {
        HighlightingRule rule;

        rule.mPattern = QRegularExpression("\\b(and|as|assert|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|"
                                          "nonlocal|not|or|pass|raise|return|try|while|with|yield|False|None|True)\\b");
        rule.mPattern.optimize();
        rule.mFormat = PythonQssAdapter::instance()->mKeywordFormat;
        mHighlightingRules.append(rule);

        rule.mPattern = QRegularExpression("=|==|!=|<|<=|>|>=|\\+|-|\\*|/|//|%|\\*\\*|\\+=|-=|\\*=|/=|%=|\\^|\\||&|~|>>|<<");
        rule.mPattern.optimize();
        rule.mFormat = PythonQssAdapter::instance()->mOperatorFormat;
        mHighlightingRules.append(rule);

        rule.mPattern = QRegularExpression("{|}|\\(|\\)|\\[|]");
        rule.mPattern.optimize();
        rule.mFormat = PythonQssAdapter::instance()->mBraceFormat;
        mHighlightingRules.append(rule);

        rule.mPattern = QRegularExpression("\\bself\\b");
        rule.mPattern.optimize();
        rule.mFormat = PythonQssAdapter::instance()->mSelfFormat;
        mHighlightingRules.append(rule);

        rule.mPattern = QRegularExpression("(\\b([1-9][0-9]*|0)[.]?[0-9]*[eE][+-]?[0-9]+[jJ]?\\b)|"// Exponent float
                                          "(\\b([1-9][0-9]*|0)\\.[0-9]*[jJ]?)|"                           // Point float
                                          "(\\b0[xX][0-9A-Fa-f]+[lL]?\\b)|"                               // Hexadecimal integer
                                          "(\\b0[oO][0-7]+[lL]?\\b)|"                                     // Octal integer
                                          "(\\b([1-9][0-9]*|0)[lLjJ]?\\b)");                              // Decimal integer

        rule.mPattern.optimize();
        rule.mFormat = PythonQssAdapter::instance()->mNumberFormat;
        mHighlightingRules.append(rule);
    }

    void PythonSyntaxHighlighter::highlightBlock(const QString& text)
    {
        for (const HighlightingRule& rule : mHighlightingRules)
        {
            QRegularExpressionMatchIterator it = rule.mPattern.globalMatch(text);
            while (it.hasNext())
            {
                QRegularExpressionMatch match = it.next();
                int index                     = match.capturedStart();
                int length                    = match.capturedLength();
                setFormat(index, length, rule.mFormat);
            }
        }

        const PythonQssAdapter* adapter = PythonQssAdapter::instance();
        auto stringFormat               = [adapter](QChar quote) -> const QTextCharFormat& { return quote == '"' ? adapter->mDoubleQuotedStringFormat : adapter->mSingleQuotedStringFormat; };

        QVector<ScanFrame> frames = decodeState(previousBlockState());
        auto openField            = [&frames]() {
            ScanFrame field;
            field.mField = true;
            frames.append(field);
        };
        const int length = text.length();
        bool continued   = false;
        int i            = 0;

        while (i < length)
        {
            const QChar c = text.at(i);

            if (frames.isEmpty() || (frames.last().mField && !frames.last().mInFormatSpec))
            {
                // code, at the top level or inside a replacement field of an f-string
                if (c == '#')
                {
                    setFormat(i, length - i, adapter->mCommentFormat);
                    break;
                }

                int quote = i;
                if (c.isLetter() || c == '_')
                {
                    while (quote < length && (text.at(quote).isLetterOrNumber() || text.at(quote) == '_'))
                    {
                        ++quote;
                    }
                    if (quote == length || (text.at(quote) != '\'' && text.at(quote) != '"') || !mStringPrefixes.contains(text.mid(i, quote - i), Qt::CaseInsensitive))
                    {
                        i = quote;
                        continue;
                    }
                }

                if (text.at(quote) == '\'' || text.at(quote) == '"')
                {
                    const QString prefix = text.mid(i, quote - i);
                    ScanFrame string;
                    string.mQuote     = text.at(quote);
                    string.mTriple    = text.mid(quote, 3) == QString(3, string.mQuote);
                    string.mFormatted = prefix.contains('f', Qt::CaseInsensitive);
                    string.mRaw       = prefix.contains('r', Qt::CaseInsensitive);
                    const int start   = quote + (string.mTriple ? 3 : 1);
                    setFormat(i, start - i, stringFormat(string.mQuote));
                    frames.append(string);
                    i = start;
                    continue;
                }

                if (!frames.isEmpty())
                {
                    ScanFrame& field = frames.last();
                    if (c == '(' || c == '[' || c == '{')
                    {
                        ++field.mBracketDepth;
                    }
                    else if ((c == ')' || c == ']' || c == '}') && field.mBracketDepth > 0)
                    {
                        --field.mBracketDepth;
                    }
                    else if (c == '}')
                    {
                        frames.removeLast();
                    }
                    else if (c == ':' && field.mBracketDepth == 0)
                    {
                        field.mInFormatSpec = true;
                    }
                }
                ++i;
            }
            else if (frames.last().mField)
            {
                // a format spec is text of its f-string, apart from the fields nested in it
                if (c == '{')
                {
                    openField();
                }
                else if (c == '}')
                {
                    frames.removeLast();
                }
                else
                {
                    int owner = frames.size() - 1;
                    while (owner > 0 && frames.at(owner).mField)
                    {
                        --owner;
                    }
                    setFormat(i, 1, stringFormat(frames.at(owner).mQuote));
                }
                ++i;
            }
            else
            {
                const ScanFrame string = frames.last();
                int step               = 1;

                if (c == string.mQuote && (!string.mTriple || text.mid(i, 3) == QString(3, c)))
                {
                    step = string.mTriple ? 3 : 1;
                    frames.removeLast();
                }
                else if (c == '\\')
                {
                    const QChar next = i + 1 < length ? text.at(i + 1) : QChar();
                    if (next.isNull())
                    {
                        continued = true;
                    }
                    // a backslash does not escape a brace in an f-string
                    else if (string.mFormatted && (next == '{' || next == '}'))
                    {
                        step = 1;
                    }
                    // the braces of a \N{...} escape are not a replacement field
                    else if (string.mFormatted && !string.mRaw && next == 'N' && i + 2 < length && text.at(i + 2) == '{')
                    {
                        const int close = text.indexOf('}', i + 3);
                        step            = (close < 0 ? length : close + 1) - i;
                    }
                    else
                    {
                        step = 2;
                    }
                }
                else if (string.mFormatted && (c == '{' || c == '}'))
                {
                    if (i + 1 < length && text.at(i + 1) == c)
                    {
                        step = 2;
                    }
                    else if (c == '{')
                    {
                        openField();
                        ++i;
                        continue;
                    }
                }
                setFormat(i, step, stringFormat(string.mQuote));
                i += step;
            }
        }

        // a string that is not triple quoted ends with its line, unless the line ends in a backslash
        if (!frames.isEmpty() && !frames.last().mField && !frames.last().mTriple && !continued)
        {
            frames.removeLast();
        }

        setCurrentBlockState(encodeState(frames));
    }

    int PythonSyntaxHighlighter::encodeState(const QVector<ScanFrame>& frames)
    {
        // 3 bits for the number of frames, then 5 bits for each frame, outermost first
        const int count = qMin(int(frames.size()), 5);
        int state       = count;
        for (int k = 0; k < count; ++k)
        {
            const ScanFrame& frame = frames.at(k);
            const int bits =
                frame.mField ? 1 | frame.mInFormatSpec << 1 | qMin(frame.mBracketDepth, 7) << 2 : (frame.mQuote == '"') << 1 | frame.mTriple << 2 | frame.mFormatted << 3 | frame.mRaw << 4;
            state |= bits << (3 + 5 * k);
        }
        return state;
    }

    QVector<PythonSyntaxHighlighter::ScanFrame> PythonSyntaxHighlighter::decodeState(int state)
    {
        QVector<ScanFrame> frames;
        if (state <= 0)
        {
            return frames;
        }

        for (int k = 0; k < (state & 7); ++k)
        {
            const int bits = state >> (3 + 5 * k) & 31;
            ScanFrame frame;
            frame.mField = bits & 1;
            if (frame.mField)
            {
                frame.mInFormatSpec = bits & 2;
                frame.mBracketDepth = bits >> 2;
            }
            else
            {
                frame.mQuote     = bits & 2 ? '"' : '\'';
                frame.mTriple    = bits & 4;
                frame.mFormatted = bits & 8;
                frame.mRaw       = bits & 16;
            }
            frames.append(frame);
        }
        return frames;
    }
}

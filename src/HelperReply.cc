/*
 * DEBUG: section 84    Helper process maintenance
 * AUTHOR: Amos Jeffries
 */
#include "squid.h"
#include "HelperReply.h"
#include "helper.h"
#include "SquidString.h"

HelperReply::HelperReply(const char *buf, size_t len, bool urlQuoting) :
        result(HelperReply::Unknown),
        whichServer(NULL)
{
    // check we have something to parse
    if (!buf || len < 1) {
        // for now ensure that legacy handlers are not presented with NULL strings.
        other_.init(1,1);
        other_.terminate();
        return;
    }

    const char *p = buf;

    // optimization: do not consider parsing result code if the response is short.
    // URL-rewriter may return relative URLs or empty response for a large portion
    // of its replies.
    if (len >= 2) {
        // some helper formats (digest auth, URL-rewriter) just send a data string
        // we must also check for the ' ' character after the response token (if anything)
        if (!strncmp(p,"OK",2) && (len == 2 || p[2] == ' ')) {
            result = HelperReply::Okay;
            p+=2;
        } else if (!strncmp(p,"ERR",3) && (len == 3 || p[3] == ' ')) {
            result = HelperReply::Error;
            p+=3;
        } else if (!strncmp(p,"BH",2) && (len == 2 || p[2] == ' ')) {
            result = HelperReply::BrokenHelper;
            p+=2;
        } else if (!strncmp(p,"TT ",3)) {
            // NTLM challenge token
            result = HelperReply::TT;
            p+=3;
            // followed by an auth token
            char *token = strwordtok(NULL, &p);
            authToken.init();
            authToken.append(token, strlen(token));
        } else if (!strncmp(p,"AF ",3)) {
            // NTLM/Negotate OK response
            result = HelperReply::OK;
            p+=3;
            // followed by:
            //  an auth token and user field
            // or, an optional username field
            char *blob = strwordtok(NULL, &p);
            char *arg = strwordtok(NULL, &p);
            if (arg != NULL) {
                authToken.init();
                authToken.append(blob, strlen(blob));
                user.init();
                user.append(arg,strlen(arg));
            } else if (blob != NULL) {
                user.init();
                user.append(blob, strlen(blob));
            }
        } else if (!strncmp(p,"NA ",3)) {
            // NTLM fail-closed ERR response
            result = HelperReply::NA;
            p+=3;
        }

        for (; xisspace(*p); ++p); // skip whitespace
    }

    const mb_size_t blobSize = (buf+len-p);
    other_.init(blobSize+1, blobSize+1);
    other_.append(p, blobSize); // remainders of the line.

    // NULL-terminate so the helper callback handlers do not buffer-overrun
    other_.terminate();

    bool found;
    do {
        found = false;
        found |= parseKeyValue("tag=", 4, tag);
        found |= parseKeyValue("user=", 5, user);
        found |= parseKeyValue("password=", 9, password);
        found |= parseKeyValue("message=", 8, message);
        found |= parseKeyValue("log=", 8, log);
        found |= parseKeyValue("token=", 8, authToken);
    } while(found);

    if (urlQuoting) {
        // unescape the reply values
        if (tag.hasContent())
            rfc1738_unescape(tag.buf());
        if (user.hasContent())
            rfc1738_unescape(user.buf());
        if (password.hasContent())
            rfc1738_unescape(password.buf());
        if (message.hasContent())
            rfc1738_unescape(message.buf());
        if (log.hasContent())
            rfc1738_unescape(log.buf());
    }
}

bool
HelperReply::parseKeyValue(const char *key, size_t key_len, MemBuf &value)
{
    if (other().contentSize() > static_cast<mb_size_t>(key_len) && memcmp(other().content(), key, key_len) == 0) {
        // parse the value out of the string. may be double-quoted
        char *tmp = modifiableOther().content() + key_len;
        const char *token = strwordtok(NULL, &tmp);
        value.reset();
        value.append(token,strlen(token));
        const mb_size_t keyPairSize = tmp - other().content();
        modifiableOther().consume(keyPairSize);
        modifiableOther().consumeWhitespace();
        return true;
    }
    return false;
}

std::ostream &
operator <<(std::ostream &os, const HelperReply &r)
{
    os << "{result=";
    switch(r.result) {
    case HelperReply::Okay:
        os << "OK";
        break;
    case HelperReply::Error:
        os << "ERR";
        break;
    case HelperReply::BrokenHelper:
        os << "BH";
        break;
    case HelperReply::TT:
        os << "TT";
        break;
    case HelperReply::NA:
        os << "NA";
        break;
    case HelperReply::Unknown:
        os << "Unknown";
        break;
    }

    if (r.other().hasContent())
        os << ", other: \"" << r.other().content() << '\"';

    os << '}';

    return os;
}

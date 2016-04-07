//
//  String.c
//  Emojicode
//
//  Created by Theo Weidmann on 02.02.15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#include "EmojicodeString.h"

#include <string.h>
#include "utf8.h"
#include "EmojicodeList.h"

EmojicodeInteger stringCompare(String *a, String *b) {
    if (a == b) {
        return 0;
    }
    if (a->length != b->length) {
        return a->length - b->length;
    }
    
    return memcmp(a->characters->value, b->characters->value, a->length * sizeof(EmojicodeChar));
}

bool stringEqual(String *a, String *b){
    return stringCompare(a, b) == 0;
}

bool stringBeginsWith(String *a, String *with){
    if(a->length < with->length){
        return false;
    }
    
    return memcmp(a->characters->value, with->characters->value, with->length * sizeof(EmojicodeChar)) == 0;
}

bool stringEndsWith(String *a, String *end){
    if(a->length < end->length){
        return false;
    }
    
    return memcmp(((EmojicodeChar*)a->characters->value) + (a->length - end->length), end->characters->value, end->length * sizeof(EmojicodeChar)) == 0;
}

/** @warning GC-invoking */
Object* stringSubstring(Object *stro, EmojicodeInteger from, EmojicodeInteger length, Thread *thread){
    stackPush(stro, 1, 0, thread);
    {
        String *string = stackGetThis(thread)->value;
        if (from >= string->length){
            length = 0;
            from = 0;
        }
        
        if (length + from >= string->length){
            length = string->length - from;
        }
        
        if(length == 0){
            stackPop(thread);
            return emptyString;
        }
    }
    
    stackSetVariable(0, somethingObject(newObject(CL_STRING)), thread);
    Object *co = newArray(length * sizeof(EmojicodeChar));
    
    Object *ostro = stackGetVariable(0, thread).object;
    String *ostr = ostro->value;
    
    ostr->length = length;
    ostr->characters = co;
    
    memcpy(ostr->characters->value, characters((String *)stackGetThis(thread)->value) + from, length * sizeof(EmojicodeChar));
    
    stackPop(thread);
    return ostro;
}

void initStringFromSymbolList(Object *string, List *list){
    String *str = string->value;
    
    size_t count = list->count;
    str->length = count;
    str->characters = newArray(count * sizeof(EmojicodeChar));
    
    for (size_t i = 0; i < count; i++) {
        characters(str)[i] = (EmojicodeChar)listGet(list, i).raw;
    }
}

//MARK: Converting from C strings

char* stringToChar(String *str){
    //Size needed for UTF8 representation
    size_t ds = u8_codingsize(characters(str), str->length);
    //Allocate space for the UTF8 string
    char *utf8str = malloc(ds + 1);
    //Convert
    size_t written = u8_toutf8(utf8str, ds, characters(str), str->length);
    utf8str[written] = 0;
    return utf8str;
}

Object* stringFromChar(const char *cstring){
    EmojicodeInteger len = u8_strlen(cstring);
    
    if(len == 0){
        return emptyString;
    }
    
    Object *stro = newObject(CL_STRING);
    String *string = stro->value;
    string->length = len;
    string->characters = newArray(len * sizeof(EmojicodeChar));
    
    u8_toucs(characters(string), len, cstring, strlen(cstring));
    
    return stro;
}

//MARK: Bridges

static Something stringPrintStdoutBrigde(Thread *thread){
    String *string = stackGetThis(thread)->value;
    char *utf8str = stringToChar(string);
    printf("%s\n", utf8str);
    free(utf8str);
    return NOTHINGNESS;
}

static Something stringEqualBridge(Thread *thread){
    String *a = stackGetThis(thread)->value;
    String *b = stackGetVariable(0, thread).object->value;
    return stringEqual(a, b) ? EMOJICODE_TRUE : EMOJICODE_FALSE;
}

static Something stringSubstringBridge(Thread *thread){
    EmojicodeInteger from = unwrapInteger(stackGetVariable(0, thread));
    EmojicodeInteger length = unwrapInteger(stackGetVariable(1, thread));
    String *string = stackGetThis(thread)->value;
    
    if (from < 0) {
        from = (EmojicodeInteger)string->length + from;
    }
    if (length < 0) {
        length = (EmojicodeInteger)string->length + length;
    }
    
    return somethingObject(stringSubstring(stackGetThis(thread), from, length, thread));
}

static Something stringSearchBridge(Thread *thread){
    String *string = stackGetThis(thread)->value;
    String *search = stackGetVariable(0, thread).object->value;
    
    for (EmojicodeInteger i = 0; i < string->length; ++i){
        bool found = true;
        for (EmojicodeInteger j = 0; j < search->length; ++j) {
            if (characters(string)[i + j] != characters(search)[j]) {
                found = false;
                break;
            }
        }
        
        if (found) {
            return somethingInteger(i);
        }
    }
    return NOTHINGNESS;
}

static Something stringTrimBridge(Thread *thread){
    String *string = stackGetThis(thread)->value;
    
    EmojicodeInteger start = 0;
    EmojicodeInteger stop = string->length - 1;
    
    while(start < string->length && isWhitespace(characters(string)[start]))
        start++;
    
    while(stop > 0 && isWhitespace(characters(string)[stop]))
        stop--;
    
    return somethingObject(stringSubstring(stackGetThis(thread), start, stop - start + 1, thread));
}

static void stringGetInput(Thread *thread) {
    String *prompt = stackGetVariable(0, thread).object->value;
    char *utf8str = stringToChar(prompt);
    printf("%s\n", utf8str);
    fflush(stdout);
    free(utf8str);
    
    int bufferSize = 50, oldBufferSize = 0;
    Object *buffer = newArray(bufferSize);
    size_t bufferUsedSize = 0;
    
    while (true) {
        fgets((char *)buffer->value + oldBufferSize, bufferSize - oldBufferSize, stdin);
        
        bufferUsedSize = strlen(buffer->value);
        
        if(bufferUsedSize < bufferSize - 1){
            if (((char *)buffer->value)[bufferUsedSize - 1] == '\n') {
                bufferUsedSize -= 1;
            }
            break;
        }
        
        oldBufferSize = bufferSize - 1;
        bufferSize *= 2;
        buffer = resizeArray(buffer, bufferSize);
    }

    EmojicodeInteger len = u8_strlen_l(buffer->value, bufferUsedSize);
    
    String *string = stackGetThis(thread)->value;
    string->length = len;
    
    Object *chars = newArray(len * sizeof(EmojicodeChar));
    string = stackGetThis(thread)->value;
    string->characters = chars;
    
    u8_toucs(characters(string), len, buffer->value, bufferUsedSize);
}

static Something stringSplitByStringBridge(Thread *thread) {
    Something sp = stackGetVariable(0, thread);
    
    stackPush(stackGetThis(thread), 2, 0, thread);
    stackSetVariable(1, somethingObject(newObject(CL_LIST)), thread);
    stackSetVariable(0, sp, thread);
    
    EmojicodeInteger firstOfSeperator = 0, seperatorIndex = 0, firstAfterSeperator = 0;
    
    for (EmojicodeInteger i = 0, l = ((String *)stackGetThis(thread)->value)->length; i < l; i++) {
        Object *stringObject = stackGetThis(thread);
        Object *separatorObject = stackGetVariable(0, thread).object;
        String *separator = (String *)separatorObject->value;
        if(characters((String *)stringObject->value)[i] == characters(separator)[seperatorIndex]){
            if (seperatorIndex == 0) {
                firstOfSeperator = i;
            }
            
            //Full seperator found
            if(firstOfSeperator + separator->length == i + 1){
                Object *stro;
                if(firstAfterSeperator == firstOfSeperator){
                    stro = emptyString;
                }
                else {
                    stro = stringSubstring(stringObject, firstAfterSeperator, i - firstAfterSeperator - separator->length + 1, thread);
                }
                listAppend(stackGetVariable(1, thread).object, somethingObject(stro), thread);
                seperatorIndex = 0;
                firstAfterSeperator = i + 1;
            }
            else {
                seperatorIndex++;
            }
        }
        else if(seperatorIndex > 0){ //and not matching
            seperatorIndex = 0;
            //We search from the begin of the last partial match
            i = firstOfSeperator;
        }
    }
    
    Object *stringObject = stackGetThis(thread);
    String *string = (String *)stringObject->value;
    listAppend(stackGetVariable(1, thread).object, somethingObject(stringSubstring(stringObject, firstAfterSeperator, string->length - firstAfterSeperator, thread)), thread);
    
    Something list = stackGetVariable(1, thread);
    stackPop(thread);
    return list;
}

static Something stringLengthBridge(Thread *thread){
    String *string = stackGetThis(thread)->value;
    return somethingInteger((EmojicodeInteger)string->length);
}

static Something stringUTF8LengthBridge(Thread *thread){
    String *str = stackGetThis(thread)->value;
    return somethingInteger((EmojicodeInteger)u8_codingsize(str->characters->value, str->length));
}

static Something stringByAppendingSymbolBridge(Thread *thread){
    Object *co = newArray((((String *)stackGetThis(thread)->value)->length + 1) * sizeof(EmojicodeChar));
    String *string = stackGetThis(thread)->value;
    
    Object *ostro = newObject(CL_STRING);
    String *ostr = ostro->value;
    
    ostr->length = string->length + 1;
    ostr->characters = co;
    
    memcpy(characters(ostr), characters(string), string->length * sizeof(EmojicodeChar));
    
    characters(ostr)[string->length] = unwrapSymbol(stackGetVariable(0, thread));
    
    return somethingObject(ostro);
}

static Something stringSymbolAtBridge(Thread *thread){
    EmojicodeInteger index = unwrapInteger(stackGetVariable(0, thread));
    String *str = stackGetThis(thread)->value;
    if(index >= str->length){
        return NOTHINGNESS;
    }
    
    return somethingInteger(characters(str)[index]);
}

static Something stringBeginsWithBridge(Thread *thread){
    return stringBeginsWith(stackGetThis(thread)->value, stackGetVariable(0, thread).object->value) ? EMOJICODE_TRUE : EMOJICODE_FALSE;
}

static Something stringEndsWithBridge(Thread *thread){
    return stringEndsWith(stackGetThis(thread)->value, stackGetVariable(0, thread).object->value) ? EMOJICODE_TRUE : EMOJICODE_FALSE;
}

static Something stringSplitBySymbolBridge(Thread *thread){
    EmojicodeChar separator = unwrapSymbol(stackGetVariable(0, thread));
    stackPush(stackGetThis(thread), 1, 0, thread);
    stackSetVariable(0, somethingObject(newObject(CL_LIST)), thread);
    
    EmojicodeInteger from = 0;
    
    for (EmojicodeInteger i = 0, l = ((String *)stackGetThis(thread)->value)->length; i < l; i++) {
        Object *stringObject = stackGetThis(thread);
        if (characters((String *)stringObject->value)[i] == separator) {
            listAppend(stackGetVariable(0, thread).object, somethingObject(stringSubstring(stringObject, from, i - from, thread)), thread);
            from = i + 1;
        }
        
    }

    Object *stringObject = stackGetThis(thread);
    listAppend(stackGetVariable(0, thread).object, somethingObject(stringSubstring(stringObject, from, ((String *) stringObject->value)->length - from, thread)), thread);
    
    Something list = stackGetVariable(0, thread);
    stackPop(thread);
    return list;
}

static Something stringToData(Thread *thread){
    String *str = stackGetThis(thread)->value;
    
    size_t ds = u8_codingsize(characters(str), str->length);
    
    Object *bytesObject = newArray(ds);
    
    str = stackGetThis(thread)->value;
    u8_toutf8(bytesObject->value, ds, characters(str), str->length);
    
    stackPush(bytesObject, 0, 0, thread);
    
    Object *o = newObject(CL_DATA);
    Data *d = o->value;
    d->length = ds;
    d->bytesObject = stackGetThis(thread);
    d->bytes = d->bytesObject->value;
    
    stackPop(thread);
    
    return somethingObject(o);
}

static Something stringToCharacterList(Thread *thread){
    String *str = stackGetThis(thread)->value;
    Object *list = newObject(CL_LIST);
    
    for (size_t i = 0; i < str->length; i++) {
        listAppend(list, somethingSymbol(characters(str)[i]), thread);
    }
    return somethingObject(list);
}

static Something stringJSON(Thread *thread){
    return parseJSON(thread);
}

static void stringFromSymbolListBridge(Thread *thread){
    initStringFromSymbolList(stackGetThis(thread), stackGetVariable(0, thread).object->value);
}

static void stringFromStringList(Thread *thread) {
    size_t stringSize = 0;
    size_t appendLocation = 0;
    
    {
        List *list = stackGetVariable(0, thread).object->value;
        String *glue = stackGetVariable(1, thread).object->value;
        
        for (size_t i = 0; i < list->count; i++) {
            stringSize += ((String *)listGet(list, i).object->value)->length;
        }
        
        if (list->count > 0){
            stringSize += glue->length * (list->count - 1);
        }
    }
        
    Object *co = newArray(stringSize * sizeof(EmojicodeChar));
    
    {
        List *list = stackGetVariable(0, thread).object->value;
        String *glue = stackGetVariable(1, thread).object->value;
        
        String *string = stackGetThis(thread)->value;
        string->length = stringSize;
        string->characters = co;
        
        for (size_t i = 0; i < list->count; i++) {
            String *aString = listGet(list, i).object->value;
            memcpy(characters(string) + appendLocation, characters(aString), aString->length * sizeof(EmojicodeChar));
            appendLocation += aString->length;
            if(i + 1 < list->count){
                memcpy(characters(string) + appendLocation, characters(glue), glue->length * sizeof(EmojicodeChar));
                appendLocation += glue->length;
            }
        }
    }
}

static void stringFromSymbol(Thread *thread){
    Object *co = newArray(sizeof(EmojicodeChar));
    
    String *string = stackGetThis(thread)->value;
    string->length = 1;
    string->characters = co;
    
    ((EmojicodeChar *)string->characters->value)[0] = (EmojicodeChar)stackGetVariable(0, thread).raw;
}

static void stringFromInteger(Thread *thread){
    EmojicodeInteger base = stackGetVariable(1, thread).raw;
    EmojicodeInteger n = stackGetVariable(0, thread).raw, a = llabs(n);
    bool negative = n < 0;
    
    EmojicodeInteger d = negative ? 2 : 1;
    while (n /= base) d++;
    
    Object *co = newArray(d * sizeof(EmojicodeChar));
    
    String *string = stackGetThis(thread)->value;
    string->length = d;
    string->characters = co;
    
    EmojicodeChar *characters = characters(string) + d;
    do
        *--characters =  "0123456789abcdefghijklmnopqrstuvxyz"[a % base % 35];
    while (a /= base);
    
    if (negative) characters[-1] = '-';
}

static Something stringToInteger(Thread *thread){
    EmojicodeInteger base = stackGetVariable(0, thread).raw;
    String *string = (String *)stackGetThis(thread)->value;
    
    if (string->length == 0) {
        return NOTHINGNESS;
    }
    
    EmojicodeChar *characters = characters(string);
    
    EmojicodeInteger x = 0;
    for (size_t i = 0; i < string->length; i++) {
        if (i == 0 && (characters[i] == '-' || characters[i] == '+')) {
            if (string->length < 2) {
                return NOTHINGNESS;
            }
            continue;
        }
        
        EmojicodeInteger b = base;
        if ('0' <= characters[i] && characters[i] <= '9') {
            b = characters[i] - '0';
        }
        else if ('A' <= characters[i] && characters[i] <= 'Z') {
            b = characters[i] - 'A' + 10;
        }
        else if ('a' <= characters[i] && characters[i] <= 'z') {
            b = characters[i] - 'a' + 10;
        }
        
        if (b >= base) {
            return NOTHINGNESS;
        }
        
        x *= base;
        x += b;
    }
    
    if (characters[0] == '-') {
        x *= -1;
    }
    return somethingInteger(x);
}

static void stringFromData(Thread *thread){
    Data *data = stackGetVariable(0, thread).object->value;
    if (!u8_isvalid(data->bytes, data->length)) {
        stackGetThis(thread)->value = NULL;
        return;
    }
    
    EmojicodeInteger len = u8_strlen_l(data->bytes, data->length);
    Object *characters = newArray(len * sizeof(EmojicodeChar));
    
    String *string = stackGetThis(thread)->value;
    string->length = len;
    string->characters = characters;
    
    data = stackGetVariable(0, thread).object->value;
    
    u8_toucs(characters(string), len, data->bytes, data->length);
}

static Something stringCompareBridge(Thread *thread) {
    String *a = stackGetThis(thread)->value;
    String *b = stackGetVariable(0, thread).object->value;
    return somethingInteger(stringCompare(a, b));
}

#define FNV_PRIME_64 1099511628211
#define FNV_OFFSET_64 14695981039346656037U
EmojicodeInteger stringHash(String *str) {
    EmojicodeInteger hash = FNV_OFFSET_64;
    for(size_t i = 0; i < str->length; i++){
        hash = hash ^ characters(str)[i];
        hash = hash * FNV_PRIME_64;
    }
    return hash;
}

static Something stringHashBridge(Thread *thread) {
    String *str = stackGetThis(thread)->value;
    return somethingInteger(stringHash(str));
}

void stringMark(Object *self){
    if(((String *)self->value)->characters){
        mark(&((String *)self->value)->characters);
    }
}

MethodHandler stringMethodForName(EmojicodeChar name){
    switch (name) {
        case 0x1F600:
            return stringPrintStdoutBrigde;
        case 0x1F61B:
            return stringEqualBridge;
        case 0x1F4CF:
            return stringLengthBridge;
        case 0x1F4DD:
            return stringByAppendingSymbolBridge;
        case 0x1F52C:
            return stringSymbolAtBridge;
        case 0x1F52A:  //🔪
            return stringSubstringBridge;
        case 0x1F50D: //🔍
            return stringSearchBridge;
        case 0x1F527: //🔧
            return stringTrimBridge;
        case 0x1F52B: //🔫
            return stringSplitByStringBridge;
        case 0x1F4D0: //📐
            return stringUTF8LengthBridge;
        case 0x1F4A3: //💣
            return stringSplitBySymbolBridge;
        case 0x1F3BC: //🎼
            return stringBeginsWithBridge;
        case 0x26F3: //⛳️
            return stringEndsWithBridge;
        case 0x1F3B6: //🎶
            return stringToCharacterList;
        case 0x1F4C7: //📇
            return stringToData;
        case 0x1f4f0: //📰
            return stringJSON;
        case 0x1F682: //🚂
            return stringToInteger;
        case 0x2194: //↔️
            return stringCompareBridge;
        case 0x2702: //✂️
            return stringHashBridge;
    }
    return NULL;
}

InitializerHandler stringInitializerForName(EmojicodeChar name){
    switch (name) {
        case 0x1F62F: //😮
            return stringGetInput;
        case 0x1F399: //🎙
            return stringFromSymbolListBridge;
        case 0x1F523:
            return stringFromSymbol;
        case 0x1F368: //🍨
            return stringFromStringList;
        case 0x1F682: //🚂
            return stringFromInteger;
        case 0x1F4C7:
            return stringFromData;
    }
    return NULL;
}

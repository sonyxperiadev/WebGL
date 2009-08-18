/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <stdlib.h>
#include <string.h>

using namespace std;

string WEBKITLIB = "external/webkit";

string oldBaseStr;
string oldCmdStr;
string newBaseStr;
string newCmdStr;
string sandboxBaseStr;
string sandboxCmdStr;
string outputDir;
string scratchDir;

const char* oldBase;
const char* oldCmd;
const char* newBase;
const char* newCmd;
const char* sandboxBase;
const char* sandboxCmd;

bool assert_debug;

#define myassert(a) do { \
    if (!(a)) { \
        fprintf(stderr, "%s %d %s\n", __FUNCTION__, __LINE__, #a); \
        fflush(stderr); \
        if (assert_debug) for(;;); else exit(0); \
    } \
} while(false)

class Options {
public:
    Options() : emitGitCommands(false), emitPerforceCommands(false), 
        mergeMake(true), copyOther(true), mergeCore(true),
        removeEmptyDirs(true), removeSVNDirs(true), debug(false), 
        execute(false), verbose(false), cleared(false)
    {
    }
    
    bool finish();
    void clearOnce()
    {
        if (cleared)
            return;
        mergeMake = copyOther = mergeCore = removeEmptyDirs = removeSVNDirs = false;
        cleared = true;
    }
    string androidWebKit;
    string baseWebKit;
    string newWebKit;
    bool emitGitCommands;
    bool emitPerforceCommands;
    bool mergeMake;
    bool copyOther;
    bool mergeCore;
    bool removeEmptyDirs;
    bool removeSVNDirs;
    bool debug;
    bool execute;
    bool verbose;
private:
    bool cleared;
};

Options options;

char* GetFile(string fileNameStr, size_t* sizePtr = NULL, int* lines = NULL) 
{
    const char* fileName = fileNameStr.c_str();
    FILE* file = fopen(fileName, "r");
    if (file == NULL)
    {
        fprintf(stderr, "can't read %s\n", fileName);
        myassert(0);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    if (sizePtr)
        *sizePtr = size;
    fseek(file, 0, SEEK_SET);
    char* buffer = new char[size + 2];
    fread(buffer, size, 1, file);
    buffer[size] = buffer[size + 1] = '\0';
    int lineCount = 0;
    for (size_t index = 0; index < size; index++) {
        if (buffer[index] == '\n') {
            buffer[index] = '\0';
            lineCount++;
        }
    }
    if (lines)
        *lines = lineCount;
    fclose(file);
    return buffer;
}

bool Options::finish()
{
    ::assert_debug = options.debug;
    if (androidWebKit.size() == 0) {
        fprintf(stderr, "missing --android parameter");
        return false;
    }
    if (baseWebKit.size() == 0) {
        fprintf(stderr, "missing --basewebkit parameter");
        return false;
    }
    if (newWebKit.size() == 0) {
        fprintf(stderr, "missing --newwebkit parameter");
        return false;
    }
    sandboxBaseStr = androidWebKit + "/" + WEBKITLIB;
    sandboxCmdStr = sandboxBaseStr;
    int err = system("pwd > pwd.txt");
    myassert(err != -1);
    outputDir = string(GetFile("pwd.txt"));
    system("rm pwd.txt");
    myassert(outputDir.size() > 0);
    scratchDir = outputDir;
    outputDir += "/scripts/";
    string outputMkdir = "test -d " + outputDir + " || mkdir " + outputDir;
    system(outputMkdir.c_str());
    scratchDir += "/scratch/";
    string scratchMkdir = "test -d " + scratchDir + " || mkdir " + scratchDir;
    system(scratchMkdir.c_str());
    oldBaseStr = baseWebKit;
    oldCmdStr = oldBaseStr;
    newBaseStr = newWebKit;
    newCmdStr = newBaseStr;
    oldBase = oldBaseStr.c_str();
    oldCmd = oldCmdStr.c_str();
    newBase = newBaseStr.c_str();
    newCmd = newCmdStr.c_str();
    sandboxBase = sandboxBaseStr.c_str();
    sandboxCmd = sandboxCmdStr.c_str();
    return true;
}

// scratch files
string ScratchFile(const char* name)
{
    return scratchDir + name + ".txt";
}

FILE* commandFile;
FILE* copyDirFile;
FILE* oopsFile;

string SedEscape(const char* str)
{
    string result;
    char ch;
    while ((ch = *str++) != '\0') {
        if (ch == '/' || ch == '\\' || ch == '$')
            result += '\\';
        result += ch;
    }
    return result;
}

char* List(const char* base, char* name, const char* workingDir)
{
    string listStr = "ls -F \"";
    listStr += string(base) + "/" + workingDir + "\" > " + ScratchFile(name);
    int err = system(listStr.c_str());
    myassert(err == 0);
    return GetFile(ScratchFile(name));
}

bool Merge(const char* oldDir, const char* oldFile, const char* newDir, const char* newFile, 
    const char* outFile)
{
    char scratch[2048];
    
    sprintf(scratch, "merge -p -q \"%s/%s/%s\" \"%s/%s/%s\"  \"%s/%s/%s\" > %s", 
            sandboxBase, oldDir, oldFile, oldBase, oldDir, oldFile, newBase, newDir, newFile, outFile);
    int err = system(scratch);
    myassert(err == 0 || err ==1 || err == 256);
    return err == 0;
}

bool Merge(const char* dir, const char* file)
{
    return Merge(dir, file, dir, file, "/dev/null");
}

/*
static const char* skipNonSpace(char** linePtr) {
    char* line = *linePtr;
    while (line[0] && isspace(line[0]) == false)
        line++;
    *linePtr = line;
    return line;
}
*/

static bool endsWith(const char* str, const char* end) {
	size_t endLen = strlen(end);
	const char* endStr = str + strlen(str) - endLen;
	return endStr >= str && strcmp(endStr, end) == 0;
}

static void skipSpace(char** linePtr) {
    char* line = *linePtr;
    while (isspace(line[0]))
        line++;
    *linePtr = line;
}

/*
static void setTrimmed(string& str, char* loc, int len) {
    char* start = loc;
    skipSpace(&loc);
    len -= loc - start;
    while (len > 0 && isspace(loc[len - 1]))
        len--;
    str = string(loc, len);
}
*/

static bool skipText(char** linePtr, const char* text) {
    skipSpace(linePtr);
    size_t length = strlen(text);
    bool result = strncmp(*linePtr, text, length) == 0;
    if (result)
        *linePtr += length;
    skipSpace(linePtr);
    return result;
}

static bool isTokenChar(char ch) {
    return isalnum(ch) || ch == '_';
}

struct Pair {
    char* name;
    int brace;
};

class Parse {
public:
    char* m_text;
    bool m_inComment;
    bool m_inFunction;
    bool m_inFindFunctionType;
    vector<Pair> m_classes;
    vector<Pair> m_namespaces;
    vector<Pair> m_preprocessorConditions;
    string m_functionName;
    string m_functionDeclaration;
    string m_classDeclaration;
    int m_braceLevel;
    int m_functionBrace;
    
    Parse(char* text) : m_text(text), m_inComment(false), m_inFunction(false), m_inFindFunctionType(false),
        m_braceLevel(0), m_functionBrace(0) {}
        
    int CheckForBrace()
    {
        int indent = 0;
        // don't count braces in strings, chars, comments
        do {
            char* openBrace = strchr(m_text, '{');
            char* closeBrace = strchr(m_text, '}');
            if (openBrace == NULL && closeBrace == NULL)
                break;
            char* brace = openBrace == NULL ? closeBrace : closeBrace == NULL ? openBrace : 
                openBrace < closeBrace ? openBrace : closeBrace;
            char* doubleQ = strchr(m_text, '"');
            char* singleQ = strchr(m_text, '\'');
            char* quote = doubleQ == NULL ? singleQ : singleQ == NULL ? doubleQ : 
                doubleQ < singleQ ? doubleQ : singleQ;
            char quoteMark = quote == doubleQ ? '"' : '\'';
            if (quote && quote < brace) {
                myassert(quote[-1] != '\\');
                do {
                    quote = strchr(quote + 1, quoteMark);
                    myassert(doubleQ);
                } while (quote[-1] == '\\');
                m_text = quote + 1;
                continue;
            }
            indent += openBrace != NULL ? 1 : -1;
            m_text = openBrace + 1;
        } while (m_text[0] != '\0');
        return indent;
    }

int ParseLine()
{
    size_t textLen = strlen(m_text);
    char* lineStart = m_text;
    char* commentStart;
    if (m_text[0] == '\0')
        goto nextLine;
    if (m_inComment == false && m_text[0] == '/') {
        m_inComment = m_text[1] == '*';
        if (m_text[1] == '/' || m_text[1] == '*')
            goto nextLine;
    }
    commentStart = m_text; // before anything else, turn embedded comments into their own lines
    if (m_inComment == false && skipText(&commentStart, "//")) {
        if (commentStart < lineStart + textLen)
            commentStart[0] = '/';
        else
            commentStart[-1] = ' ';
        commentStart -= 2;
        commentStart[0] = '\0';
       textLen = commentStart - lineStart;
       goto nextLine;
    }
    if (m_inComment || skipText(&commentStart, "/*")) {
        char* commentEnd = commentStart;
        if (skipText(&commentEnd, "*/")) {
            if (commentEnd < lineStart + textLen) {
                commentEnd[-1] = '\0';
                textLen = commentEnd - lineStart - 2;
            }
            if (m_inComment) {
                m_inComment = false;
                goto nextLine;
            }
        }
         if (m_inComment)
            goto nextLine;
        memcpy(commentStart - 2, "\0/*", 3);
        textLen = commentStart - lineStart - 2;
   }
    if (skipText(&m_text, "#include")) 
        goto nextLine;
    if (skipText(&m_text, "#if") || skipText(&m_text, "#else")) {
        Pair condition = { lineStart, m_braceLevel };
        m_preprocessorConditions.push_back(condition);
        goto nextLine;
    }
    {
        bool is_endif = false;
        if (skipText(&m_text, "#elif") || (is_endif = skipText(&m_text, "#endif")) != false) { // pop prior elif, if
            char* lastText;
            do {
                int last = m_preprocessorConditions.size() - 1;
                myassert(last >= 0);
                lastText = m_preprocessorConditions[last].name;
                m_preprocessorConditions.pop_back();
            } while (is_endif && skipText(&lastText, "#else") == true);
            goto nextLine;
        }
    }
    if (skipText(&m_text, "namespace")) {
        Pair space = { lineStart, m_braceLevel };
        m_namespaces.push_back(space);
        goto checkForBrace;
    }
    if (m_inFunction)
        goto checkForBrace;
    // detect functions by looking for token preceding open-paren.
    if (m_inFindFunctionType == false) {
        char* openParen = strchr(m_text, '(');
        if (openParen) {
            char* last = openParen;
            while (last > m_text && isspace(last[-1]))
                --last;
            while (last > m_text && ((isTokenChar(last[-1]) || last[-1] == ':') && last[-2] == ':'))
                --last;
            myassert(isdigit(last[0]) == false);
            if (isTokenChar(last[0])) {
                m_inFindFunctionType = true;
                m_functionName = string(last);
            }
        }
    }
    {
        char* openBrace = strchr(m_text, '{');
        char* semiColon = strchr(m_text, ';');
        if  (semiColon != NULL && semiColon < openBrace)
            openBrace = NULL;
    // functions are of the form: type (class::)*function(parameter-list) {
        // all of which may be on separate lines
        // so keep track of returntype, class, function, paramlist, openbrace separately
        if (m_inFindFunctionType == true) { // look ahead to see which comes first, a semicolon or an open brace
            if (openBrace) {
                m_functionBrace = ++m_braceLevel;
                m_inFunction = true;
                m_inFindFunctionType = false;
                m_text = openBrace + 1;
                goto checkForBrace;
            }
            if (semiColon != NULL) { // a function declaration
                m_inFindFunctionType = false;
                m_functionDeclaration = m_functionName;
                m_functionName.erase(0, m_functionName.length());
            } else
                goto nextLine;
        }
        // FIXME what if class line has neither brace nor semi?
        if (skipText(&m_text, "class")) {
            if (openBrace > m_text) {
                Pair _class = { lineStart, m_braceLevel };
                m_classes.push_back(_class);
            } else if (semiColon != NULL) {
                m_classDeclaration = lineStart; // !!! FIXME should have function form as above
            }
        }
    }
checkForBrace:
    m_braceLevel += CheckForBrace();
    if (m_functionBrace > 0 && m_braceLevel <= m_functionBrace) {
        m_functionName.erase(0, m_functionName.length());
        m_functionBrace = 0;
    }
nextLine:
    return textLen;
}

};

char* const GetAndroidDiffs(const char* dir, const char* filename)
{
    char scratch[2048];
    string diffsFile = ScratchFile(__FUNCTION__);
    sprintf(scratch, "diff \"%s/%s/%s\"  \"%s/%s/%s\" > %s", sandboxBase, dir, 
        filename, oldBase, dir, filename, diffsFile.c_str());
    int err = system(scratch);
    myassert(err == 0 || err == 256);
    char* const diffs = GetFile(diffsFile);
    return diffs;
}

void CheckForExec(const char* base1, const char* dir1, const char* file1, 
    const char* base2, const char* dir2, const char* file2, bool* ex1, bool* ex2) 
{
    size_t file1Len = strlen(file1);
    size_t file2Len = strlen(file2);
    bool file1Ex = file1[file1Len - 1] == '*';
    bool file2Ex = file2[file2Len - 1] == '*';
    if (file1Ex != file2Ex) {
        fprintf(stderr, "warning: %s/%s/%s has %sexec bit set while"
            " %s/%s/%s has %sexec bit set\n", 
            base1, dir1, file1, file1Ex ? "" : "no ", 
            base2, dir2, file2, file2Ex ? "" : "no ");
    }
    if (ex1) *ex1 = file1Ex;
    if (ex2) *ex2 = file2Ex;
}

bool CompareFiles(const char* base1, const char* dir1, const char* file1, 
     const char* base2, const char* dir2, const char* file2)
{
    char scratch[2048];
    bool file1Ex, file2Ex;
    string compareFileStr = ScratchFile(__FUNCTION__);
    CheckForExec(base1, dir1, file1, base2, dir2, file2, &file1Ex, &file2Ex);
    sprintf(scratch, "diff --brief \"%s/%s/%.*s\" \"%s/%s/%.*s\" > %s", 
        base1, dir1, (int) strlen(file1) - (int) file1Ex, file1, 
        base2, dir2, (int) strlen(file2) - (int) file2Ex, file2, 
        compareFileStr.c_str());
    int err = system(scratch);
    myassert(err == 0 || err == 256);
    char* scratchText = GetFile(compareFileStr);
    size_t len = strlen(scratchText);
    delete[] scratchText;
    return len > 0;
}

bool CompareFiles(const char* base1, const char* base2, const char* dir, const char* file)
{
    return CompareFiles(base1, dir, file, base2, dir, file);
}

int Compare(char* one, size_t len1, char* two, size_t len2)
{
    char* o_end = one + len1;
    char* t_end = two + len2;
    do {
        if (one == o_end)
            return two == t_end ? 0 : 1;
        if (two == t_end)
            return -1;
        char o = *one++;
        char t = *two++;
        if (o == t)
            continue;
        if (o == '/')
            return -1;
        if (t == '/')
            return 1;
        return o < t ? -1 : 1;
    } while (true);
    myassert(0);
    return 0;
}

string Find(const char* oldList)
{
    string result;
    char scratch[2048];
    // look in WebCore and JavaScriptCore
    string findWebCore = ScratchFile("FindWebCore");
    sprintf(scratch, "cd %s%s ;  find . -name \"%s\" > %s", 
            newBase, "/WebCore", oldList, findWebCore.c_str());
    int err = system(scratch);
    myassert(err == 0 || err == 256);
    int webCount;
    char* foundInWebCore = GetFile(findWebCore, NULL, &webCount);
    char* originalFoundInWebCore = foundInWebCore;
    string findJavaScriptCore = ScratchFile("FindJavaScriptCore");
    sprintf(scratch, "cd %s%s ;  find . -name \"%s\" > %s", 
            newBase, "/JavaScriptCore", oldList, findJavaScriptCore.c_str());
    err = system(scratch);
    myassert(err == 0 || err == 256);
    int javaScriptCount;
    char* foundInJavaScriptCore = GetFile(findJavaScriptCore, NULL, &javaScriptCount);
    char* originalFoundInJavaScriptCore = foundInJavaScriptCore;
    if (webCount == 1 && javaScriptCount == 0) {
        result = "WebCore/" + string(&foundInWebCore[2]);
    } else if (webCount == 0 && javaScriptCount == 1) {
        result = "JavaScriptCore/" + string(&foundInJavaScriptCore[2]);
    } else if (webCount == 1 && javaScriptCount == 1 && 
            strncmp(&foundInWebCore[2], "ForwardingHeaders/", 18) == 0) {
        result = "JavaScriptCore/" + string(&foundInJavaScriptCore[2]);
    } else if (webCount == 1 && javaScriptCount == 1 && 
            strncmp(&foundInJavaScriptCore[2], "API/tests/", 10) == 0) {
        result = "JavaScriptCore/" + string(&foundInJavaScriptCore[2]);
    } else if (webCount + javaScriptCount > 0) {
        fprintf(stderr, "deleted file \"%s\" has more than one possible rename:\n", oldList);
        int index;
        for (index = 0; index < webCount; index++) {
            fprintf(stderr, "WebCore/%s\n", &foundInWebCore[2]);
            foundInWebCore += strlen(foundInWebCore) + 1;
        }
        for (index = 0; index < javaScriptCount; index++) {
            fprintf(stderr, "JavaScriptCore/%s\n", &foundInJavaScriptCore[2]);
            foundInJavaScriptCore += strlen(foundInJavaScriptCore) + 1;
        }
    }
    delete[] originalFoundInWebCore;
    delete[] originalFoundInJavaScriptCore;
    return result;
}

char* GetMakeAndExceptions(const char* dir, const char* filename, size_t* makeSize,
    string* excludedFilesPtr, string* excludedGeneratedPtr, 
    string* excludedDirsPtr, string* androidFilesPtr,
    char** startPtr, char** localStartPtr)
{
    char scratch[1024];
    sprintf(scratch, "%s/%s/%s", sandboxBase, dir, filename);
    char* makeFile = GetFile(scratch, makeSize);
    char* start = makeFile;
    do { // find first filename in makefile
        if (strncmp(start, "# LOCAL_SRC_FILES_EXCLUDED := \\", 30) == 0)
            break;
        start += strlen(start) + 1;
    } while (start < makeFile + *makeSize);
    myassert(start[0] != '\0');
    start += strlen(start) + 1;
    // construct one very large regular expression that looks like:
    // echo '%s' | grep -v -E 'DerivedSources.cpp|WebCorePrefix.cpp...'
    // to filter out matches that aren't allowed
    // wildcards '*' in the original need to be expanded to '.*'
    string excludedFiles = "grep -v -E '";
    while (strncmp(start, "#\t", 2) == 0) {
        start += 2;
        char ch;
        while ((ch = *start++) != ' ') {
            if (ch == '*')
                excludedFiles += '.';
            else if (ch == '.')
                excludedFiles += '\\';
            excludedFiles += ch;
        }
        excludedFiles += "|";
        myassert(*start == '\\');
        start += 2;
    }
    excludedFiles[excludedFiles.size() - 1] = '\'';
    *excludedFilesPtr = excludedFiles;
    do {
        if (strncmp(start, "# LOCAL_GENERATED_FILES_EXCLUDED := \\", 37) == 0 ||
                strncmp(start, "# LOCAL_DIR_WILDCARD_EXCLUDED := \\", 34) == 0)
            break;
        start += strlen(start) + 1;
    } while (start < makeFile + *makeSize);
    if (strncmp(start, "# LOCAL_GENERATED_FILES_EXCLUDED := \\", 37) == 0) {
        string excludedGenerated = "grep -v -E '";
        start += strlen(start) + 1;
        while (strncmp(start, "#\t", 2) == 0) {
            start += 2;
            char ch;
            while ((ch = *start++) != ' ') {
                if (ch == '*')
                    excludedGenerated += '.';
                else if (ch == '.')
                    excludedGenerated += '\\';
                excludedGenerated += ch;
            }
            excludedGenerated += "|";
            myassert(*start == '\\');
            start += 2;
        }
        myassert(excludedGeneratedPtr);
        excludedGenerated[excludedGenerated.size() - 1] = '\'';
        *excludedGeneratedPtr = excludedGenerated;
    }
    do { // find first filename in makefile
        if (strncmp(start, "# LOCAL_DIR_WILDCARD_EXCLUDED := \\", 34) == 0)
            break;
        start += strlen(start) + 1;
    } while (start < makeFile + *makeSize);
    if (start[0] != '\0') {
        string excludedDirs = "-e '/\\.vcproj\\// d' -e '/\\.svn\\// d' ";
        do {
            start += strlen(start) + 1;
            char* exceptionDirStart = start;
            if (strncmp(exceptionDirStart, "#\t", 2) != 0) {
                myassert(exceptionDirStart[0] == '\0');
                break;
            }
            exceptionDirStart += 2;
            char* exceptionDirEnd = exceptionDirStart;
            do {
                exceptionDirEnd = strchr(exceptionDirEnd, '\\');
            } while (exceptionDirEnd && *++exceptionDirEnd == '/');
            myassert(exceptionDirEnd);
            --exceptionDirEnd;
            myassert(exceptionDirEnd[-1] == ' ');
            myassert(exceptionDirEnd[-2] == '*');
            myassert(exceptionDirEnd[-3] == '/');
            exceptionDirEnd[-3] = '\0';
            excludedDirs += "-e '/";
            if (exceptionDirStart[0] == '/')
                excludedDirs += "\\";
            excludedDirs += exceptionDirStart;
            excludedDirs += "\\// d' ";
            start = exceptionDirEnd;
        } while (true);
        *excludedDirsPtr = excludedDirs;
    }
    *startPtr = start;
    // optionally look for android-specific files
    char* makeEnd = makeFile + *makeSize;
    do { // find first filename in makefile
        if (strcmp(start, "# LOCAL_ANDROID_SRC_FILES_INCLUDED := \\") == 0)
            break;
    } while ((start += strlen(start) + 1), start < makeEnd);
    if (start >= makeEnd)
        return makeFile;
    start += strlen(start) + 1;
    string androidFiles = "grep -v -E '";
    do {
        myassert(strncmp(start, "#\t", 2) == 0);
        start += 2;
        char ch;
        bool isIdl = strstr(start, "idl \\") != 0;
        char* lastSlash = strrchr(start, '/') + 1;
        while ((ch = *start++) != ' ') {
            if (ch == '*')
                androidFiles += '.';
            else if (ch == '.')
                androidFiles += '\\';
            androidFiles += ch;
            if (!isIdl)
                continue;
            if (ch == '/' && start == lastSlash)
                androidFiles += "JS";
            if (ch == '.') {
                myassert(strcmp(start, "idl \\") == 0);
                start += 4;
                androidFiles += 'h';
                break;
            }
        }
        androidFiles += "|";
        myassert(*start == '\\');
        start += 2;
    } while (start[0] == '#');
    androidFiles[androidFiles.size() - 1] = '\'';
    *androidFilesPtr = androidFiles;
    return makeFile;
}

vector<char*> GetDerivedSourcesMake(const char* dir)
{
    vector<char*> result;
    char scratch[1024];
    sprintf(scratch, "%s/%s/%s", newBase, dir, "DerivedSources.make");
    size_t fileSize;
    char* file = GetFile(scratch, &fileSize);
    char* fileEnd = file + fileSize;
    myassert(file);
    size_t len;
    do { // find first filename in makefile
        len = strlen(file);
        if (strcmp(file, "all : \\") == 0)
            break;
        file += len + 1;
    } while (file < fileEnd);
    myassert(strcmp(file, "all : \\") == 0);
    file += len + 1;
    while (file[0] != '#') {
        len = strlen(file);
        char* st = file;
        skipSpace(&st);
        if (st[0] != '\\' && st[0] != '$')
            result.push_back(st);
        file += len + 1;
    }
    return result;
}

bool MarkDerivedFound(vector<char*>& derived, char* check, size_t len)
{
    bool found = false;
    for (unsigned index = 0; index < derived.size(); index++) {
        char* der = derived[index];
        if (strncmp(der, check, len) == 0) {
            der[0] = '\0';
            found = true;
        }
    }
    return found;
}

void UpdateDerivedMake()
{
    const char* dir = "WebCore";
    int err;
    vector<char*> derived = GetDerivedSourcesMake(dir);
    size_t makeSize;
    char* start, * localStart;
    string excludedDirs, excludedFiles, excludedGenerated, androidFiles;
    char* makeFile = GetMakeAndExceptions(dir, "Android.derived.mk", &makeSize,
        &excludedFiles, &excludedGenerated, &excludedDirs, &androidFiles, 
        &start, &localStart);
    if (options.emitPerforceCommands)
        fprintf(commandFile, "p4 edit %s/%s/%s\n", sandboxCmd, dir, "Android.derived.mk");
    fprintf(commandFile, "cat %s/%s/%s | sed \\\n", sandboxCmd, dir, "Android.derived.mk");
    string updateDerivedMake = ScratchFile(__FUNCTION__);
    string filelist = string("cd ") + newBase + "/" + dir + 
        " ;  find . -name '*.idl' | " + excludedFiles + 
        " | sed -e 's/.\\///' " + excludedDirs + 
        " | sed 's@\\(.*\\)/\\(.*\\)\\.idl@    $(intermediates)/\\1/JS\\2.h@' "
        " | sort -o " + updateDerivedMake; 
    err = system(filelist.c_str());
    myassert(err == 0 || err == 256);
    char* bindings = GetFile(updateDerivedMake);
    bool inGen = false;
    char* fileEnd = makeFile + makeSize;
    char* nextStart;
    do {
        size_t startLen = strlen(start);
        nextStart = start + startLen + 1;
        bool onGen = false;
        char* st = start;
        if (inGen == false) {
            if (strncmp(st, "GEN", 3) != 0)
                continue;
            st += 3;
            skipSpace(&st);
            if (strncmp(st, ":=", 2) != 0)
                continue;
            st += 2;
            onGen = true;
        }
        skipSpace(&st);
        inGen = start[startLen - 1] == '\\';
        if (inGen) {
            if (st[0] == '\\' && st[1] == '\0')
                continue;
            if (strcmp(st, "$(addprefix $(intermediates)/, \\") == 0)
                continue;
        } else if (st[0] == ')' && st[1] == '\0')
            continue;
        static const char bindHead[] = "bindings/js/";
        const size_t bindLen = sizeof(bindHead) - 1;
        string escaped;
        if (strncmp(st, bindHead, bindLen) == 0) {
            st += bindLen;
            if (MarkDerivedFound(derived, st, strlen(st)) == false) {
                fprintf(stderr, "*** webkit removed js binding: %s"
                    " (must be removed manually)\n", st);
                escaped = SedEscape(st);
                fprintf(commandFile, "-e '/%s/ d' ", escaped.c_str());
            } 
            continue;
        }
        myassert(strncmp(st, "$(intermediates)", 16) == 0);
        if (onGen && inGen == false) { // no continuation
            char* lastSlash;
            myassert(strrchr(st, '/') != NULL);
            while ((lastSlash = strrchr(st, '/')) != NULL) {
                char* lastEnd = strchr(lastSlash, ' ');
                if (lastEnd == NULL)
                    lastEnd = lastSlash + strlen(lastSlash);
                myassert(lastSlash != 0);
                lastSlash++;
                size_t lastLen = lastEnd - lastSlash;
                if (MarkDerivedFound(derived, lastSlash, lastLen) == false) {
                    fprintf(stderr, "*** webkit removed generated file:"
                        " %.*s (must be removed manually)\n", (int) lastLen,
                        lastSlash);
              //      escaped = SedEscape(st);
              //      fprintf(commandFile, "-e '/%s/ d' \\\n", escaped.c_str());
                }
                char* priorDollar = strrchr(st, '$');
                myassert(priorDollar != NULL);
                char* nextDollar = strchr(st, '$');
                if (nextDollar == priorDollar)
                    break;
                priorDollar[0] = '\0';
            }
            continue;
        }
        char* nextSt = nextStart;
        skipSpace(&nextSt);
        myassert(strncmp(nextSt, "$(intermediates)", 16) != 0 || strcmp(st, nextSt) < 0);
   //     do {
        char* bind = bindings;
        myassert(bind);
        skipSpace(&bind);
        int compare = strncmp(bind, st, strlen(bind) - 2);
        if (compare < 0) { // add a file
            escaped = SedEscape(st);
            char* filename = strrchr(bindings, '/');
            myassert(filename);
            filename += 3;
            // FIX ME: exclude items in DerivedSources.make all : $(filter-out ...
            char* bi = bindings;
            skipSpace(&bi);
            char* bindName = strrchr(bi, '/');
            myassert(bindName != NULL);
            bindName++;
            string biStr = SedEscape(bi);
            fprintf(commandFile, "-e '/%s/ i\\\n_TAB_%s \\\\\n' ", 
                escaped.c_str(), biStr.c_str());
            MarkDerivedFound(derived, bindName, strlen(bindName));
            nextStart = start;
            bindings += strlen(bindings) + 1;
            inGen = true;
        } else if (compare > 0) {
            // if file to be deleted is locally added by android, leave it alone
            myassert(strncmp(st, "$(intermediates)/", 17) == 0);
            string subst = string(st).substr(17, strlen(st) - 17 - (inGen ? 2 : 0));
            string localDerivedStr = ScratchFile("LocalDerived");
            string filter = string("echo '") + subst + "' | " + androidFiles +
                " > " + localDerivedStr;
            if (options.debug)
                fprintf(stderr, "LocalDerived.txt : %s\n", filter.c_str());
            err = system(filter.c_str());
            myassert(err == 0 || err == 256);
            char* localDerived = GetFile(localDerivedStr);
            if (localDerived[0] != '\0') {
                escaped = SedEscape(st);
                fprintf(commandFile, "-e '/%s/ d' ", escaped.c_str());
            }
        } else {
            char* stName = strrchr(st, '/');
            myassert(stName);
            stName++;
            MarkDerivedFound(derived, stName, strlen(stName));
            bindings += strlen(bindings) + 1;
        }
     //   } while (strstr(start, "$(intermediates)") != NULL);
        // if changing directories, add any new files to the end of this directory first
        if (bindings[0] != '\0' && strstr(nextStart, "$(intermediates)") == NULL) {
            st = start;
            skipSpace(&st);
            escaped = SedEscape(st);
            st = strchr(st, '/');
            char* stDirEnd = strchr(st + 1, '/');
            do {
                bind = strchr(bindings, '/');
                if (!bind)
                    break;
                char* bindEnd = strchr(bind + 1, '/');
                if (!bindEnd)
                    break;
                if (bindEnd - bind != stDirEnd - st)
                    break;
                if (strncmp(st, bind, stDirEnd - st) != 0)
                    break;
                if (inGen == false)
                    fprintf(commandFile, "-e '/%s/ s/$/ \\\\/' ", escaped.c_str());
                char* bi = bindings;
                skipSpace(&bi);
                string biStr = SedEscape(bi);
                fprintf(commandFile, "-e '/%s/ a\\\n_TAB_%s\n' ", 
                    escaped.c_str(), biStr.c_str());
                MarkDerivedFound(derived, bindEnd + 1, strlen(bindEnd + 1));
                escaped = biStr;
                inGen = false;
                bindings += strlen(bindings) + 1;
            } while (true);
        }
    } while (start = nextStart, start < fileEnd);
    for (unsigned index = 0; index < derived.size(); index++) {
        char* der = derived[index];
        if (der[0] == '\0')
            continue;
        string excludedGeneratedStr = ScratchFile("ExcludedGenerated");
        string filter = string("echo '") + der + "' | " + excludedGenerated +
            " > " + excludedGeneratedStr;
        err = system(filter.c_str());
        myassert(err == 0 || err == 256);
        char* excluded = GetFile(excludedGeneratedStr);
        if (excluded[0] != '\0')
            fprintf(stderr, "*** missing rule to generate %s\n", der);
    }
    fprintf(commandFile, " | sed 's/^_TAB_/\t/' > %s/%s/%s\n", sandboxCmd, dir, "xAndroid.derived.mk");
    fprintf(commandFile, "mv  %s/%s/%s %s/%s/%s\n", 
        sandboxCmd, dir, "xAndroid.derived.mk", sandboxCmd, dir, "Android.derived.mk");
    if (options.emitGitCommands)
        fprintf(commandFile, "git add %s/%s\n", dir, "Android.derived.mk");
}

int MatchLen(const char* one, const char* two, size_t len)
{
    bool svgIn1 = strstr(one, "svg") || strstr(one, "SVG");
    bool svgIn2 = strstr(two, "svg") || strstr(two, "SVG");
    if (svgIn1 != svgIn2)
        return 0;
    int signedLen = (int) len;
    int original = signedLen;
    while (*one++ == *two++ && --signedLen >= 0)
        ;
    return original - signedLen;
}

// create the list of sed commands to update the WebCore Make file
void UpdateMake(const char* dir)
{
    // read in the makefile
    size_t makeSize;
    char* start, * localStart = NULL;
    string excludedDirs, excludedFiles, androidFiles;
    char* makeFile = GetMakeAndExceptions(dir, "Android.mk", &makeSize,
        &excludedFiles, NULL, &excludedDirs, &androidFiles, &start, &localStart);
    char* lastFileName = NULL;
    size_t lastFileNameLen = 0;
    int lastLineNumber = -1;
    // get the actual list of files
    string updateMakeStr = ScratchFile(__FUNCTION__);
    string filelist = string("cd ") + newBase + "/" + dir + " ;" 
        "  find . -name '*.cpp' -or -name '*.c' -or -name '*.y' | " +
        excludedFiles + " |  sed -e 's/.\\///' " + excludedDirs + 
        " | sort -o " + updateMakeStr;
    if (options.debug)
        fprintf(stderr, "make %s/%s filter: %s\n", dir, "Android.mk", filelist.c_str());
    int err = system(filelist.c_str());
    myassert(err == 0 || err == 256);
    char* newList = GetFile(updateMakeStr);
    do { // find first filename in makefile
        if (strncmp(start, "LOCAL_SRC_FILES := \\", 20) == 0)
            break;
        start += strlen(start) + 1;
    } while (start < makeFile + makeSize);
    myassert(start[0] != '\0');
    if (options.emitPerforceCommands)
        fprintf(commandFile, "p4 edit %s/%s/%s\n", sandboxCmd, dir, "Android.mk");
    fprintf(commandFile, "cat %s/%s/%s | sed ", sandboxCmd, dir, "Android.mk");
    int lineNumber = 0;
    do {
        start += strlen(start) + 1;
        lineNumber++;
        if (start - makeFile >= makeSize || start[0] == '$')
            break;
        if (start[0] == '\0' || !isspace(start[0]))
            continue;
        skipSpace(&start);
        if (start[0] == '\0' || start[0] == '\\')
            continue;
        size_t startLen = strlen(start);
        if (start[startLen - 1] == '\\')
            --startLen;
        while (isspace(start[startLen - 1]))
            --startLen;
        size_t newListLen = strlen(newList);
        if (lastFileName != NULL) {
            myassert(strncmp(start, lastFileName, startLen) > 0 || 
                startLen > lastFileNameLen);
        }
        if (strstr(start, "android") != NULL || strstr(start, "Android") != NULL) {
            if (startLen == newListLen && strncmp(newList, start, startLen) == 0)
                newList += newListLen + 1;
            lastFileName = start;
            lastFileNameLen = startLen;
            lastLineNumber = lineNumber;
            continue;
        }
        int compare;
        bool backslash = lastFileName &&
            lastFileName[strlen(lastFileName) - 1] == '\\';
        do {
            compare = strncmp(newList, start, startLen);
            if (compare == 0 && startLen != newListLen)
                compare = newListLen < startLen ? -1 : 1;
            if (newList[0] == '\0' || compare >= 0) 
                break;
            // add a file
            if (lastFileName && lineNumber - lastLineNumber > 1 &&
                    MatchLen(lastFileName, newList, lastFileNameLen) >
                    MatchLen(start, newList, startLen)) {
                string escaped = SedEscape(lastFileName);
                if (!backslash)
                    fprintf(commandFile, "-e '/%s/ s/$/ \\\\/' ", escaped.c_str());
                fprintf(commandFile, "-e '/%s/ a\\\n_TAB_%s%s\n' ", 
                    SedEscape(lastFileName).c_str(), newList, 
                    backslash ? " \\\\" : "");
                lastFileName = newList;
                lastFileNameLen = newListLen;
            } else {
                fprintf(commandFile, "-e '/%s/ i\\\n_TAB_%s \\\\\n' ", 
                    SedEscape(start).c_str(), newList);
            }
            newList += newListLen + 1;
            newListLen = strlen(newList);
        } while (true);
        if (newList[0] == '\0' || compare > 0) {
            // don't delete files added by Android
            string localMakeStr = ScratchFile("LocalMake");
            string filter = "echo '" + string(start).substr(0, startLen) + 
                "' | " + androidFiles + " > " + localMakeStr;
            int err = system(filter.c_str());
            myassert(err == 0 || err == 256);
            char* localMake = GetFile(localMakeStr);
            if (localMake[0] != '\0') { 
                string escaped = SedEscape(start);
                fprintf(commandFile, "-e '/%s/ d' ", escaped.c_str());
            }
        } else
            newList += newListLen + 1;
        lastFileName = start;
        lastFileNameLen = startLen;
        lastLineNumber = lineNumber;
    } while (true);
    fprintf(commandFile, " | sed 's/^_TAB_/\t/' > %s/%s/%s\n", sandboxCmd, dir, "xAndroid.mk");
    fprintf(commandFile, "mv  %s/%s/%s %s/%s/%s\n", 
        sandboxCmd, dir, "xAndroid.mk", sandboxCmd, dir, "Android.mk");
    if (options.emitGitCommands)
        fprintf(commandFile, "git add %s/%s\n", dir, "Android.mk");
}

static bool emptyDirectory(const char* base, const char* work, const char* dir) {
    string findEmptyStr = "find \"";
    string emptyDirStr = ScratchFile("emptyDirectory");
    if (base[0] != '\0')
        findEmptyStr += string(base) + "/" + work + "/" + dir + "\"";
    else
        findEmptyStr += string(work) + "/" + dir + "\"";
    findEmptyStr += " -type f -print > " + emptyDirStr;
    int err = system(findEmptyStr.c_str());
    if (err != 0)
        return true;
    FILE* file = fopen(emptyDirStr.c_str(), "r");
    if (file == NULL)
    {
        fprintf(stderr, "can't read %s\n", emptyDirStr.c_str());
        myassert(0);
        return true;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fclose(file);
    return size == 0;
}

static bool emptyDirectory(const char* work, const char* dir) {
    return emptyDirectory("", work, dir);
}

void CompareDirs(const char* workingDir, bool renamePass)
{
    map<string, string> renameMap;
    char* oldList = List(oldBase, "old", workingDir);
    char* newList = List(newBase, "new", workingDir);
    char* sandList = List(sandboxBase, "sandbox", workingDir);
    char* oldMem = oldList;
    char* newMem = newList;
    char* sandboxMem = sandList;
    // identify files to be added, removed by comparing old, new lists
    do {
        size_t oldLen = strlen(oldList);
        size_t newLen = strlen(newList);
        size_t sandLen = strlen(sandList);
        if (oldLen == 0 && newLen == 0 && sandLen == 0)
            break;
        bool oldDir = false;
        bool oldExecutable = false;
        if (oldLen > 0) {
            char last = oldList[oldLen - 1];
            oldDir = last ==  '/';
            oldExecutable = last ==  '*';
            if (oldDir || oldExecutable)
                --oldLen;
        }
        bool newDir = false;
        bool newExecutable = false;
        if (newLen > 0) {
            char last = newList[newLen - 1];
            newDir = last ==  '/';
            newExecutable = last ==  '*';
            if (newDir || newExecutable)
                --newLen;
        }
        bool sandDir = false;
        bool sandExecutable = false;
        if (sandLen > 0) {
            char last = sandList[sandLen - 1];
            sandDir = last ==  '/';
            sandExecutable = last ==  '*';
            if (sandDir || sandExecutable)
                --sandLen;
        }
        string oldFileStr = string(oldList).substr(0, oldLen);
        const char* oldFile = oldFileStr.c_str();
        string newFileStr = string(newList).substr(0, newLen);
        const char* newFile = newFileStr.c_str();
        string sandFileStr = string(sandList).substr(0, sandLen);
        const char* sandFile = sandFileStr.c_str();
        int order = Compare(oldList, oldLen, newList, newLen);
        int sandOrder = 0;
        if ((oldLen > 0 || sandLen > 0) && order <= 0) {
            sandOrder = Compare(sandList, sandLen, oldList, oldLen);
            if (sandOrder > 0 && renamePass == false)
                fprintf(stderr, "error: file in old webkit missing from sandbox: %s/%s\n", 
                        workingDir, oldFile);
            if (sandOrder < 0 && renamePass == false) {
                // file added by android -- should always have name 'android?'
                const char* android = strstr(sandFile, "ndroid");
                if (android == NULL)
                    fprintf(stderr, "warning: expect added %s to contain 'android': %s/%s\n",
                            sandDir ? "directory" : "file" , workingDir, sandFile);
            }
            if (sandOrder == 0) {
                myassert(oldDir == sandDir);
                if (oldExecutable != sandExecutable)
                    CheckForExec(oldBase, workingDir, oldList, 
                        sandboxBase, workingDir, sandList, 0, 0);
            }
            if (sandOrder <= 0)
                sandList += strlen(sandList) + 1;
            if (sandOrder < 0)
                continue;
        }
        if (order < 0) { // file in old list is not in new
            // check to see if file is read only ; if so, call p4 delete -- otherwise, just call delete
            if (oldDir == false) {
                bool modifiedFile = false;
                // check to see if android modified deleted file
                if (sandOrder == 0) {
                    string rename(workingDir);
                    rename.append("/");
                    rename.append(oldFile);
                    if (renamePass) {
                        string newName = Find(oldFile);
                        if (newName.length() > 0) {
                            map<string, string>::iterator iter = renameMap.find(rename);
                            myassert(iter == renameMap.end()); // if I see the same file twice, must be a bug
                            renameMap[rename] = newName;
                            myassert(rename != newName);
                            if (options.debug)
                                fprintf(stderr, "map %s to %s\n", rename.c_str(), newName.c_str());
                        }
                    }
                    if (renamePass == false) {
                        bool oldSandboxDiff = CompareFiles(oldBase, sandboxBase, workingDir, oldList);
                        const char* renamedDir = workingDir;
                        map<string, string>::iterator iter = renameMap.find(rename);
                        if (iter != renameMap.end()) {
                            string newName = renameMap[rename];
                            renamedDir = newName.c_str();
                            char* renamed = (char*) strrchr(renamedDir, '/');
                            *renamed++ = '\0'; // splits rename into two strings
                            if (options.emitPerforceCommands) {
                                fprintf(commandFile, "p4 integrate \"%s/%s/%s\" \"%s/%s/%s\"\n", sandboxCmd, workingDir, oldFile,
                                    sandboxCmd, renamedDir, renamed);
                                fprintf(commandFile, "p4 resolve \"%s/%s/%s\"\n", sandboxCmd, renamedDir, renamed);
                            } else if (options.emitGitCommands) {
                                fprintf(commandFile, "git mv \"%s/%s\" \"%s/%s\"\n", workingDir, oldFile,
                                    renamedDir, renamed);
                            }
                            if (oldSandboxDiff) {
                                if (options.emitPerforceCommands)
                                    fprintf(commandFile, "p4 open \"%s/%s/%s\"\n", sandboxCmd, renamedDir, renamed);
                                fprintf(commandFile,  "merge -q \"%s/%s/%s\" \"%s/%s/%s\" \"%s/%s/%s\"\n", 
                                    sandboxCmd, renamedDir, renamed, oldCmd, workingDir, oldFile, newCmd, renamedDir, renamed);
                                bool success = Merge(workingDir, oldFile, renamedDir, renamed, "/dev/null");
                                if (success == false) {
									fprintf(stderr, "*** Manual merge required: %s/%s\n", renamedDir, renamed);
									fprintf(commandFile, "cat \"%s/%s/%s\" | sed -e 's/^<<<<<<<.*$/#ifdef MANUAL_MERGE_REQUIRED/' "
										"-e 's/^=======$/#else \\/\\/ MANUAL_MERGE_REQUIRED/' "
										"-e 's/^>>>>>>>.*$/#endif \\/\\/ MANUAL_MERGE_REQUIRED/' > \"%s/%s/x%s\"\n", 
										sandboxCmd, renamedDir, renamed, sandboxCmd, renamedDir, renamed);
									fprintf(commandFile, "mv \"%s/%s/x%s\" \"%s/%s/%s\"\n",
										sandboxCmd, renamedDir, renamed, sandboxCmd, renamedDir, renamed);
                                }
                                if (options.emitGitCommands)
                                    fprintf(commandFile, "git add \"%s/%s\"\n", renamedDir, renamed);
                            } else {
                                bool oldNewDiff = CompareFiles(oldBase, workingDir, oldList, newBase, renamedDir, renamed);
                                if (oldNewDiff) {
                                    if (options.emitPerforceCommands)
                                        fprintf(oopsFile, "p4 open \"%s/%s/%s\"\n", sandboxCmd, renamedDir, renamed);
                                    fprintf(oopsFile, "cp \"%s/%s/%s\" \"%s/%s/%s\"\n",
                                            newCmd, renamedDir, renamed, sandboxCmd, renamedDir, renamed);
                                    if (options.emitGitCommands)
                                        fprintf(oopsFile, "git add \"%s/%s\"\n", renamedDir, renamed);
                                }
                            }
                        } else if (oldSandboxDiff) {
                            modifiedFile = true;
                            fprintf(stderr, "*** Modified file deleted: %s/%s\n", workingDir, oldFile);
//                                FindDeletedAndroidChanges(workingDir, oldFile);
                        }
                    } // if renamePass == false
                } // if sandOrder == 0
                if (modifiedFile) {
                    fprintf(commandFile, "cat \"%s/%s/%s\" | sed -e '1 i\\\n#ifdef MANUAL_MERGE_REQUIRED\n' "
                        "-e '$ a\\\n#endif \\/\\/ MANUAL_MERGE_REQUIRED\n' > \"%s/%s/x%s\"\n", 
                        sandboxCmd, workingDir, oldFile, sandboxCmd, workingDir, oldFile);
                    fprintf(commandFile, "mv \"%s/%s/x%s\" \"%s/%s/%s\"\n",
                        sandboxCmd, workingDir, oldFile, sandboxCmd, workingDir, oldFile);
                } else if (options.emitPerforceCommands)
                    fprintf(commandFile, "p4 delete \"%s/%s/%s\"\n", sandboxCmd, workingDir, oldFile);
                else if (options.emitGitCommands)
                    fprintf(commandFile, "git rm \"%s/%s\"\n", workingDir, oldFile);
                else
                    fprintf(commandFile, "rm \"%s/%s/%s\"\n", sandboxCmd, workingDir, oldFile);
            } else { // if oldDir != false
 // !!! FIXME               start here;
                    // check to see if old directory is empty ... (e.g., WebCore/doc)
                    // ... and/or isn't in sandbox anyway (e.g., WebCore/LayoutTests)
                        // if old directory is in sandbox (e.g. WebCore/kcanvas) that should work
                if (options.emitPerforceCommands)
                    fprintf(commandFile, "p4 delete \"%s/%s/%s/...\"\n", sandboxCmd, workingDir, oldFile);
                else if (options.emitGitCommands)
                    fprintf(commandFile, "git rm \"%s/%s/...\"\n", workingDir, oldFile);
                else
                    fprintf(commandFile, "rm \"%s/%s/%s/...\"\n", sandboxCmd, workingDir, oldFile);
                if (renamePass == false)
                    fprintf(stderr, "*** Directory deleted: %s/%s\n", workingDir, oldFile);
            }
            oldList += strlen(oldList) + 1;
            continue;
        } 
        if (order > 0) {
             if (renamePass == false) {
                string rename(workingDir);
                rename.append("/");
                rename.append(newFile);
                bool skip = false;
                for (map<string, string>::iterator iter = renameMap.begin(); iter != renameMap.end(); iter++) {
                    if (iter->second == rename) {
                        skip = true;
                        break;
                    }
                }
                if (skip == false) {
                    if (newDir) {
                        if (strcmp(sandFile, newFile) != 0 && 
                                emptyDirectory(newBase, workingDir, newFile) == false) {
                            fprintf(copyDirFile, "find \"%s/%s/%s\" -type d -print | "
                                "sed 's@%s/\\(.*\\)@mkdir %s/\\1@' | bash -s\n",
                                newCmd, workingDir, newFile, newCmd, sandboxCmd);
                            fprintf(copyDirFile, "find \"%s/%s/%s\" -type f -print | "
                                "sed 's@%s/\\(.*\\)@cp %s/\\1 %s/\\1@' | bash -s\n",
                                newCmd, workingDir, newFile, newCmd, newCmd, sandboxCmd);
                            if (options.emitPerforceCommands)
                                fprintf(copyDirFile, "find \"%s/%s/%s\" -type f -print | "
                                    "p4 -x - add\n", 
                                    sandboxCmd, workingDir, newFile);
                            else if (options.emitGitCommands)
                                fprintf(copyDirFile, "git add \"%s/%s\"\n",
                                    workingDir, newFile);
                        }
                    } else {
//                        if (emptyDirectory(sandboxBase, workingDir)) {
//                            fprintf(commandFile, "mkdir \"%s/%s\"\n", sandboxCmd, workingDir);
//                        }
                        bool edit = false;
                        size_t newLen1 = strlen(newFile);
                        for (map<string, string>::iterator iter = renameMap.begin(); iter != renameMap.end(); iter++) {
                            if (strcmp(iter->second.c_str(), workingDir) == 0) {
                                const char* first = iter->first.c_str();
                                size_t firstLen = strlen(first);
                                if (firstLen > newLen1 && strcmp(newFile, 
                                        &first[firstLen - newLen1]) == 0) {
                                    edit = true;
                                    break;
                                }
                            }
                        }
                        if (edit == false) {
                            fprintf(commandFile, "cp \"%s/%s/%s\" \"%s/%s/%s\"\n", 
                                newCmd, workingDir, newFile, sandboxCmd, workingDir, newFile);
                                if (options.emitPerforceCommands)
                                    fprintf(commandFile, "p4 add \"%s/%s/%s\"\n", sandboxCmd, workingDir, newFile);
                                else if (options.emitGitCommands)
                                    fprintf(commandFile, "git add \"%s/%s\"\n", workingDir, newFile);
                        }
                    }
                }
            }
            newList += strlen(newList) + 1;
            continue;
        }
        if (oldDir) {
            myassert(newDir);
            size_t newLen1 = strlen(workingDir) + strlen(oldList);
            char* newFile = new char[newLen1 + 1];
            sprintf(newFile, "%s/%.*s", workingDir, (int) strlen(oldList) - 1, 
                oldList);
            if (sandOrder > 0) { // file is on old and new but not sandbox
                if (emptyDirectory(newBase, newFile) == false) {
                    fprintf(copyDirFile, "find \"%s/%s\" -type d -print | "
                        "sed 's@%s/\\(.*\\)@mkdir %s/\\1@' | bash -s\n",
                        newCmd, newFile, newCmd, sandboxCmd);
                    fprintf(copyDirFile, "find \"%s/%s\" -type f -print | "
                        "sed 's@%s/\\(.*\\)@cp %s/\\1 %s/\\1@' | bash -s\n",
                        newCmd, newFile, newCmd, newCmd, sandboxCmd);
                    if (options.emitPerforceCommands)
                        fprintf(copyDirFile, "find \"%s/%s\" -type f -print | "
                            "p4 -x - add\n", 
                            sandboxCmd, newFile);
                    else if (options.emitGitCommands)
                        fprintf(copyDirFile, "git add \"%s\"", newFile);
                }
             } else
                CompareDirs(newFile, renamePass);
            delete[] newFile;
        } else {
            // at this point, the file is in both old and new webkits; see if it changed
               // ignore executables, different or not (or always copy, or do text compare? or find binary compare? )
            if (oldExecutable != newExecutable)
                fprintf(stderr, "*** %s/%s differs in the execute bit (may cause problems for perforce)\n", workingDir, oldFile);
        //            myassert(sandOrder != 0 || sandFile[sandLen - 1] == '*');
        //    Diff(oldBase, sandboxBase, workingDir, oldFile);
            bool oldNewDiff = CompareFiles(oldBase, newBase, workingDir, oldList);
            if (oldNewDiff && sandOrder == 0 && renamePass == false) { // if it changed, see if android also changed it
                if (options.emitPerforceCommands)
                    fprintf(commandFile, "p4 edit \"%s/%s/%s\"\n", sandboxCmd, workingDir, oldFile);
                bool oldSandboxDiff = CompareFiles(oldBase, sandboxBase, workingDir, oldFile);
                if (oldSandboxDiff) {
                    fprintf(commandFile,  "merge -q \"%s/%s/%s\" \"%s/%s/%s\" \"%s/%s/%s\"\n", 
                        sandboxCmd, workingDir, oldFile, oldCmd, workingDir, oldFile, newCmd, workingDir, oldFile);
                    bool success = Merge(workingDir, oldFile);
                    if (success == false) {
						fprintf(stderr, "*** Manual merge required: %s/%s\n", workingDir, oldFile);
						fprintf(commandFile, "cat \"%s/%s/%s\" | sed -e 's/^<<<<<<<.*$/#ifdef MANUAL_MERGE_REQUIRED/' "
							"-e 's/^=======$/#else \\/\\/ MANUAL_MERGE_REQUIRED/' -e 's/^>>>>>>>.*$/#endif \\/\\/ MANUAL_MERGE_REQUIRED/' > \"%s/%s/x%s\"\n", 
							sandboxCmd, workingDir, oldFile, sandboxCmd, workingDir, oldFile);
						fprintf(commandFile, "mv \"%s/%s/x%s\" \"%s/%s/%s\"\n",
							sandboxCmd, workingDir, oldFile, sandboxCmd, workingDir, oldFile);
                    }
                } else fprintf(commandFile, "cp \"%s/%s/%s\" \"%s/%s/%s\"\n", newCmd, workingDir, oldFile ,
                    sandboxCmd, workingDir, oldFile);
                if (options.emitGitCommands)
                    fprintf(commandFile, "git add \"%s/%s\"\n", workingDir, oldFile);
            }
        }
        myassert(oldLen == newLen);
        newList += strlen(newList) + 1;
        oldList += strlen(oldList) + 1;
    } while (true);
    delete[] oldMem;
    delete[] newMem;
    delete[] sandboxMem;
}

bool Ignore(char* fileName, size_t len)
{
    if (len == 0)
        return true;
    if (fileName[len - 1] =='/') 
        return true;
    if (strcmp(fileName, ".DS_Store") == 0)
        return true;
    if (strcmp(fileName, ".ignoreSVN") == 0)
        return true;
    return false;
}

void FixStar(char* fileName, size_t len)
{
    if (fileName[len - 1] =='*')
        fileName[len - 1] = '\0';
}

void FixColon(char** fileNamePtr, size_t* lenPtr)
{
    char* fileName = *fileNamePtr;
    size_t len = *lenPtr;
    if (fileName[len - 1] !=':')
        return;
    fileName[len - 1] = '\0';
    if (strncmp(fileName ,"./", 2) != 0)
        return;
    fileName += 2;
    *fileNamePtr = fileName;
    len -= 2;
    *lenPtr = len;
}

bool IgnoreDirectory(const char* dir, const char** dirList)
{
    if (dirList == NULL)
        return false;
    const char* test;
    while ((test = *dirList++) != NULL) {
        if (strncmp(dir, test, strlen(test)) == 0)
            return true;
    }
    return false;
}

static void doSystem(char* scratch)
{
    if (false) printf("%s\n", scratch);
    int err = system(scratch);
    myassert(err == 0);
}

static void copyToCommand(char* scratch, string file)
{
    doSystem(scratch);
    char* diff = GetFile(file.c_str());
    while (*diff) {
        fprintf(commandFile, "%s\n", diff);
        diff += strlen(diff) + 1;
    }
}

#define WEBKIT_EXCLUDED_DIRECTORIES \
        "-not -path \"*Tests\" " /* includes LayoutTests, PageLoadTests */ \
        "-not -path \"*Tests/*\" " /* includes LayoutTests, PageLoadTests */ \
        "-not -path \"*Site\" " /* includes BugsSite, WebKitSite */ \
        "-not -path \"*Site/*\" " /* includes BugsSite, WebKitSite */ \
        "-not -path \"./PlanetWebKit/*\" " \
        "-not -path \"./PlanetWebKit\" "

#define ANDROID_EXCLUDED_FILES \
        "-e '/^Files .* differ/ d' " \
        "-e '/^Only in .*/ d' " \
        "-e '/Android.mk/ d' " \
        "-e '/android$/ d' "

#define ANDROID_EXCLUDED_DIRS \
        "-e '/\\/JavaScriptCore\\// d' " \
        "-e '/\\/WebCore\\// d' "

#define ANDROID_EXCLUDED_DIRS_GIT \
        "-e '/ JavaScriptCore\\// d' " \
        "-e '/ WebCore\\// d' "
        
void CopyOther()
{
    string excludedFiles = ANDROID_EXCLUDED_FILES;
    if (options.emitGitCommands)
        excludedFiles += ANDROID_EXCLUDED_DIRS_GIT;
    else
        excludedFiles += ANDROID_EXCLUDED_DIRS;
    char scratch[1024];
    // directories to ignore in webkit 
    string copyOtherWebKit = ScratchFile("CopyOtherWebKit");
    sprintf(scratch, "cd %s ;  find . -type d -not -empty "
        WEBKIT_EXCLUDED_DIRECTORIES
        " > %s", newBase, copyOtherWebKit.c_str());
    doSystem(scratch);
    // directories to ignore in android
    string copyOtherAndroid = ScratchFile("CopyOtherAndroid");
    sprintf(scratch, "cd %s ;  find . -type d -not -empty "
        "-not -path \"*.git*\" "
        "-not -path \"*android*\" "
        " > %s", sandboxBase, copyOtherAndroid.c_str());
    doSystem(scratch);
    if (0) {
        string copyOtherMkDir = ScratchFile("CopyOtherMkDir");
        sprintf(scratch, "diff %s %s | sed -e 's@< \\./\\(.*\\)$"
            "@mkdir %s/\\1@' "
            "-e '/^[0-9].*/ d' "
            "-e '/>.*/ d' "
            "-e '/---/ d' "
            "-e '/\\/JavaScriptCore\\// d' "
            "-e '/\\/WebCore\\// d' "
            "> %s", copyOtherWebKit.c_str(), copyOtherAndroid.c_str(), sandboxCmd, 
            copyOtherMkDir.c_str());
        if (options.debug)
            fprintf(stderr, "%s\n", scratch);
        copyToCommand(scratch, copyOtherMkDir);
    }
    string copyOtherDiff = ScratchFile("CopyOtherDiff");
    int scratchLen = sprintf(scratch, "diff %s %s | sed -e 's@< \\./\\(.*\\)$"
        "@mkdir -p -v %s/\\1 ; find %s/\\1 -type f -depth 1 -exec cp {} %s/\\1 \";\"",
        copyOtherWebKit.c_str(), copyOtherAndroid.c_str(), sandboxCmd, newCmd, 
        sandboxCmd);
    if (options.emitGitCommands)
        scratchLen += sprintf(&scratch[scratchLen], " ; cd %s ; find ", sandboxCmd);
    else
        scratchLen += sprintf(&scratch[scratchLen], " ; find %s/", sandboxCmd);
    scratchLen += sprintf(&scratch[scratchLen], "\\1 -type f -depth 1 | ");
    if (options.emitPerforceCommands)
        scratchLen += sprintf(&scratch[scratchLen], "p4 -x - add ");
    else if (options.emitGitCommands)
        scratchLen += sprintf(&scratch[scratchLen], "xargs git add ");
    scratchLen += sprintf(&scratch[scratchLen],
        "@' -e '/^[0-9].*/ d' "
        "-e '/>.*/ d' "
        "-e '/---/ d' "
        "-e '/\\/JavaScriptCore\\// d' "
        "-e '/\\/WebCore\\// d' "
        "> %s", copyOtherDiff.c_str());
    if (options.debug)
        fprintf(stderr, "%s\n", scratch);
    copyToCommand(scratch, copyOtherDiff);
    string deleteOtherDiff = ScratchFile("DeleteOtherDiff");
    scratchLen = sprintf(scratch, "diff -r -q %s %s | sed -e "
        "'s@Only in %s/\\(.*\\)\\: \\(.*\\)@",
        newBase, sandboxBase, sandboxBase);
    if (options.emitPerforceCommands)
        scratchLen += sprintf(&scratch[scratchLen], "p4 delete %s/", sandboxCmd);
    else if (options.emitGitCommands) 
        scratchLen += sprintf(&scratch[scratchLen], "git rm ");
    else
        scratchLen += sprintf(&scratch[scratchLen], "rm %s/", sandboxCmd);
    scratchLen += sprintf(&scratch[scratchLen], "\\1/\\2@' %s > %s",
        excludedFiles.c_str(), deleteOtherDiff.c_str());
    if (options.debug)
        fprintf(stderr, "%s\n", scratch);
    copyToCommand(scratch, deleteOtherDiff);
    string addOtherDiff = ScratchFile("AddOtherDiff");
    scratchLen = sprintf(scratch, "diff -r -q %s %s | sed -e "
        "'s@Only in %s/\\(.*\\)\\: \\(.*\\)"
            "@mkdir -p -v %s/\\1 ; cp %s/\\1/\\2 %s/\\1/\\2 ; ",
            newBase, sandboxBase, newBase, sandboxCmd, newCmd, sandboxCmd);
    if (options.emitPerforceCommands)
        scratchLen += sprintf(&scratch[scratchLen],
            "p4 add %s/\\1/\\2", sandboxCmd);
    else if (options.emitGitCommands)
        scratchLen += sprintf(&scratch[scratchLen],
            "git add \\1/\\2");
    scratchLen += sprintf(&scratch[scratchLen], "@' %s > %s",
        excludedFiles.c_str(), addOtherDiff.c_str());
    if (options.debug)
        fprintf(stderr, "%s\n", scratch);
    copyToCommand(scratch, addOtherDiff);
    string editOtherDiff = ScratchFile("EditOtherDiff");
    scratchLen = sprintf(scratch, "diff -r -q %s %s | sed -e "
        "'s@Files %s/\\(.*\\) and %s/\\(.*\\) differ@",
        newBase, sandboxBase, newBase, sandboxBase);
    if (options.emitPerforceCommands)
        scratchLen += sprintf(&scratch[scratchLen],
            "p4 edit %s/\\2 ; ", sandboxCmd);
    scratchLen += sprintf(&scratch[scratchLen], "cp %s/\\1 %s/\\2 ",
       newCmd, sandboxCmd);
    if (options.emitGitCommands)
        scratchLen += sprintf(&scratch[scratchLen],
            " ; git add \\2");
    scratchLen += sprintf(&scratch[scratchLen], "@' %s > %s", 
        excludedFiles.c_str(), editOtherDiff.c_str());
    if (options.debug)
        fprintf(stderr, "%s\n", scratch);
    copyToCommand(scratch, editOtherDiff);
}

void MakeExecutable(const string& filename)
{
    string makeExScript = "chmod +x ";
    makeExScript += filename;
    int err = system(makeExScript.c_str());
    myassert(err == 0);
}

bool ReadArgs(char* const args[], int argCount)
{
    int index = 0;
    const char* toolpath = args[index++];
    // first arg is path to this executable
    // use this to build default paths
    
    for (; index < argCount; index++) {
        const char* arg = args[index];
        if (strncmp(arg, "-a", 2) == 0 || strcmp(arg, "--android") == 0) {
            index++;
            options.androidWebKit = args[index];
            continue;
        }
        if (strncmp(arg, "-b", 2) == 0 || strcmp(arg, "--basewebkit") == 0) {
            index++;
            options.baseWebKit = args[index];
            continue;
        }
        if (strncmp(arg, "-c", 2) == 0 || strcmp(arg, "--mergecore") == 0) {
            options.clearOnce();
            options.mergeCore = true;
            continue;
        }
        if (strncmp(arg, "-d", 2) == 0 || strcmp(arg, "--debug") == 0) {
            options.debug = true;
            continue;
        }
        if (strncmp(arg, "-e", 2) == 0 || strcmp(arg, "--emptydirs") == 0) {
            options.clearOnce();
            options.removeEmptyDirs = true;
            continue;
        }
        if (strncmp(arg, "-g", 2) == 0 || strcmp(arg, "--git") == 0) {
            options.emitGitCommands = true;
            if (options.emitPerforceCommands == false)
                continue;
        }
        if (strncmp(arg, "-m", 2) == 0 || strcmp(arg, "--mergemake") == 0) {
            options.clearOnce();
            options.mergeMake = true;
            continue;
        }
        if (strncmp(arg, "-n", 2) == 0 || strcmp(arg, "--newwebkit") == 0) {
            index++;
            options.newWebKit = args[index];
            continue;
        }
        if (strncmp(arg, "-o", 2) == 0 || strcmp(arg, "--copyother") == 0) {
            options.clearOnce();
            options.copyOther = true;
            continue;
        }
        if (strncmp(arg, "-p", 2) == 0 || strcmp(arg, "--perforce") == 0) {
            options.emitPerforceCommands = true;
            if (options.emitGitCommands == false)
                continue;
        }
        if (strncmp(arg, "-s", 2) == 0 || strcmp(arg, "--removesvn") == 0) {
            options.clearOnce();
            options.removeSVNDirs = true;
            continue;
        }
        if (strncmp(arg, "-v", 2) == 0 || strcmp(arg, "--verbose") == 0) {
            options.verbose = true;
            fprintf(stderr, "path: %s\n", toolpath);
            int err = system("pwd > pwd.txt");
            myassert(err != -1);
            fprintf(stderr, "pwd: %s\n", GetFile("pwd.txt"));
            system("rm pwd.txt");
            continue;
        }
        if (strncmp(arg, "-x", 2) == 0 || strcmp(arg, "--execute") == 0) {
            options.execute = true;
            continue;
        }
        if (options.emitGitCommands && options.emitPerforceCommands)
            printf("choose one of --git and --perforce\n");
        else if (strncmp(arg, "-h", 2) != 0 && strcmp(arg, "--help") != 0 && strcmp(arg, "-?") != 0)
            printf("%s not understood\n", args[index]);
        printf(
"WebKit Merge for Android version 1.1\n"
"Usage: webkitmerge -a path -b path -n path [-g or -p] [-c -d -e -m -o -s -v -x]\n"
"Options -c -e -m -o -s are set unless one or more are passed.\n"
"Leave -g and -p unset to copy, merge, and delete files outside of source control.\n"
"-a --android path     Set the Android webkit path to merge to.\n"
"-b --basewebkit path  Set the common base for Android and the newer webkit.\n"
"-c --mergecore        Create merge scripts for WebCore, JavaScriptCore .h .cpp.\n"
"-d --debug            Show debugging printfs; loop forever on internal assert.\n"
"-e --emptydirs        Remove empty directories from webkit trees.\n"
"-g --git              Emit git commands.\n"
"-h --help             Show this help.\n"
"-m --mergemake        Create merge scripts for WebCore/Android.mk,\n"
"                      WebCore/Android.derived.mk, and JavaScriptCore/Android.mk.\n"
"-n --newwebkit path   Set the webkit to merge from.\n"
"-o --copyother        Create script to copy other webkit directories.\n"
"-p --perforce         Emit perforce commands.\n"
"-s --removesvn        Remove svn directories from webkit trees.\n"
"-v --verbose          Show status printfs.\n"
"-x --execute          Execute the merge scripts.\n"
        );
        return false;
    }
    return options.finish();
}

int main (int argCount, char* const args[])
{
    if (ReadArgs(args, argCount) == false)
        return 0;
    int err;
    // First remove all .svn directories
    if (options.removeSVNDirs) {
        if (options.verbose)
            fprintf(stderr, "removing svn directories from %s\n", newBase);
        string removeSVNStr = string("find ") + newBase + 
            " -type d -name \".svn\" -print0 | xargs -0 rm -rf";
        err = system(removeSVNStr.c_str());
        myassert(err == 0);
    }
    // Remove all empty directories
    if (options.removeEmptyDirs) {
        if (options.verbose) 
            fprintf(stderr, "removing empty directories from %s, %s\n", 
                oldBase, newBase);
        string removeEmpty = string("find ") + oldBase + " " + newBase +
            " -type d -empty -delete";
        err = system(removeEmpty.c_str());
        myassert(err == 0);
    }
    if (options.mergeCore /* || options.mergeMake */) {
        if (options.verbose) 
            fprintf(stderr, "building rename map\n");
        commandFile = fopen("/dev/null", "w");
        copyDirFile = fopen("/dev/null", "w");
        CompareDirs("WebCore", true); // build rename map
        CompareDirs("JavaScriptCore", true);
        fclose(copyDirFile);
        fclose(commandFile);
    }
    if (options.mergeMake) {
        if (options.verbose) 
            fprintf(stderr, "building make.sh\n");
        string makeShell = outputDir + "make.sh";
        commandFile = fopen(makeShell.c_str(), "w");
        if (options.emitGitCommands || options.emitPerforceCommands)
            fprintf(commandFile, "cd %s\n", sandboxCmd);
        UpdateMake("WebCore");
        UpdateMake("JavaScriptCore");
        UpdateDerivedMake();
        fclose(commandFile);
        MakeExecutable(makeShell);
    }
    if (options.copyOther) {
        if (options.verbose) 
            fprintf(stderr, "building copyOther.sh\n");
        string copyOtherShell = outputDir + "copyOther.sh";
        commandFile = fopen(copyOtherShell.c_str(), "w");
        if (options.emitGitCommands || options.emitPerforceCommands)
            fprintf(commandFile, "cd %s\n", sandboxCmd);
        CopyOther();
        fclose(commandFile);
        MakeExecutable(copyOtherShell);
    }
    if (options.mergeCore) {
        if (options.verbose) 
            fprintf(stderr, "building command.sh copyDir.sh oops.sh\n");
        string commandShell = outputDir + "command.sh";
        commandFile = fopen(commandShell.c_str(), "w");
        if (options.emitGitCommands || options.emitPerforceCommands)
            fprintf(commandFile, "cd %s\n", sandboxCmd);
        string copyDirShell = outputDir + "copyDir.sh";
        copyDirFile = fopen(copyDirShell.c_str(), "w");
        if (options.emitGitCommands || options.emitPerforceCommands)
            fprintf(copyDirFile, "cd %s\n", sandboxCmd);
        string oopsShell = outputDir + "oops.sh";
        oopsFile = fopen(oopsShell.c_str(), "w");
        if (options.emitGitCommands || options.emitPerforceCommands)
            fprintf(oopsFile, "cd %s\n", sandboxCmd);
        CompareDirs("WebCore", false); // generate command script
        CompareDirs("JavaScriptCore", false);
        fclose(oopsFile);
        fclose(copyDirFile);
        fclose(commandFile);
        MakeExecutable(oopsShell);
        MakeExecutable(copyDirShell);
        MakeExecutable(commandShell);
    }
    if (options.execute) {
        if (options.mergeCore) {
            if (options.verbose) 
                fprintf(stderr, "executing command.sh\n");
            string execCommand = "cd " + options.androidWebKit + "; . " + outputDir + "command.sh";
            err = system(execCommand.c_str());
            myassert(err == 0);
            if (options.verbose) 
                fprintf(stderr, "executing copyDir.sh\n");
            string execCopy = "cd " + options.androidWebKit + "; . " + outputDir + "copyDir.sh"; 
            err = system(execCopy.c_str());
            myassert(err == 0);
        }
        if (options.mergeMake) {
            if (options.verbose) 
                fprintf(stderr, "executing make.sh\n");
            string execMake = "cd " + options.androidWebKit + "; . " + outputDir + "make.sh"; 
            err = system(execMake.c_str());
            myassert(err == 0);
        }
        if (options.copyOther) {
            if (options.verbose) 
                fprintf(stderr, "executing copyOther.sh\n");
            string execCopyOther = "cd " + options.androidWebKit + "; . " + outputDir + "copyOther.sh"; 
            err = system(execCopyOther.c_str());
            myassert(err == 0);
        }
    }
    if (options.verbose) 
        fprintf(stderr, "done!\n");
    else {
        string rmAllCmd = "rm " + scratchDir + "* ; rmdir " + scratchDir;
        err = system(rmAllCmd.c_str());
        myassert(err == 0);
    }
    return 0;
}

/* things to do:
    when inserting MANUAL_MERGE_REQUIRED, if contents is #preprocessor, balance first?
*/

#include "StringTable.h"

#include "../util/AppInterface.h"
#include "../util/MultiplayerCommon.h"
#include "../util/Directories.h"

#include <boost/filesystem/fstream.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/algorithm/string/replace.hpp>


// static(s)
const std::string StringTable::S_DEFAULT_FILENAME = "eng_stringtable.txt";
const std::string StringTable::S_ERROR_STRING = "ERROR: ";

// StringTable
StringTable::StringTable():
    m_filename(S_DEFAULT_FILENAME)
{ Load(); }

StringTable::StringTable(const std::string& filename):
    m_filename(filename)
{ Load(); }

StringTable::~StringTable()
{}

const std::string& StringTable::operator[] (std::string index) const
{
    static std::string error_retval;
    std::map<std::string, std::string>::const_iterator it = m_strings.find(index);
    return it == m_strings.end() ? error_retval = S_ERROR_STRING + index : it->second;
}

void StringTable::Load()
{
    std::ifstream ifs(m_filename.c_str());
    std::string file_contents;
    if (ifs) {
        int c;
        while ((c = ifs.get()) != std::ifstream::traits_type::eof()) {
            file_contents += c;
        }
    } else {
        Logger().errorStream() << "Could not open or read StringTable file \"" << m_filename << "\"";
        return;
    }

    using namespace boost::xpressive;

    const sregex IDENTIFIER = +_w;
    const sregex COMMENT = '#' >> *(~_n) >> _n;
    const sregex KEY = IDENTIFIER;
    const sregex SINGLE_LINE_VALUE = *(~_n);
    const sregex MULTI_LINE_VALUE = -*_;
    const sregex FILE_ =
        IDENTIFIER >>
        +(*(space | COMMENT) >>
          KEY >> _n >>
          (("'''" >> MULTI_LINE_VALUE >> "'''" >> _n) | SINGLE_LINE_VALUE >> _n));
    const sregex REFERENCE =
        "[[" >> *space >> (s1 = IDENTIFIER) >> +space >> (s2 = IDENTIFIER) >> *space >> "]]";

    smatch matches;
    if (regex_search(file_contents, matches, FILE_)) {
        smatch::nested_results_type::const_iterator it = matches.nested_results().begin();
        smatch::nested_results_type::const_iterator end = matches.nested_results().end();
        std::size_t match_index = 0;
        std::string key;
        for (; it != end; ++it) {
            if (!match_index) {
                m_language = it->str();
                ++match_index;
            } else if (it->regex_id() == KEY.regex_id() ||
                       it->regex_id() == SINGLE_LINE_VALUE.regex_id() ||
                       it->regex_id() == MULTI_LINE_VALUE.regex_id()) {
                if (match_index % 2) {
                    key = it->str();
                } else {
                    if (m_strings.find(key) == m_strings.end()) {
                        m_strings[key] = it->str();
                        boost::algorithm::replace_all(m_strings[key], "\\n", "\n");
                    } else {
                        Logger().errorStream() << "Duplicate string ID found: '" << key << "' in file: '" << m_filename << "'.  Ignoring duplicate.";
                    }
                }
                ++match_index;
            }
        }
        for (std::map<std::string, std::string>::iterator map_it = m_strings.begin(); map_it != m_strings.end(); ++map_it)
        {
            sregex_iterator it(map_it->second.begin(), map_it->second.end(), REFERENCE);
            sregex_iterator end;
            std::size_t offset = 0;
            bool b = false;
            for (; it != end; ) {
                b = true;
                const smatch& match = *it;
                std::map<std::string, std::string>::iterator map_lookup_it = m_strings.find(match[2]);
                if (map_lookup_it != m_strings.end()) {
                    std::string substitution =
                        '<' + match[1].str() + ' ' + match[2].str() + '>' + map_lookup_it->second + "</" + match[1].str() + '>';
                    map_it->second.replace(match.position() + offset, match.length(), substitution);
                    offset = match.position() + substitution.length();
                    it = sregex_iterator(map_it->second.begin() + offset,
                                         map_it->second.end(),
                                         REFERENCE);
                }
            }
        }
    } else {
        Logger().errorStream() << "StringTable file \"" << m_filename << "\" is malformed";
    }
}


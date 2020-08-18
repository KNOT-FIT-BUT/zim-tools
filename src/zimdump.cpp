/*
 * Copyright (C) 2006 Tommi Maekitalo
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include <algorithm>
#include <regex>

#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include <zim/file.h>
#include <zim/fileiterator.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <iomanip>
#include <array>
#include <cstdlib> /* smrz */
#include <dirent.h> /* smrz */

#include "arg.h"
#include "version.h"

#include <fcntl.h>
#ifdef _WIN32
# define SEPARATOR "\\"
# include <io.h>
# include <windows.h>
#else
# define SEPARATOR "/"
# include <unistd.h>
# include <sys/stat.h>
#endif

const std::string title_beg = "<title>"; /* smrz */
const std::string title_end = "</title>"; /* smrz */
const int max_file_size = 100 * 1024 * 1024; /* smrz */

static bool isReservedUrlChar(const char c)
{
    constexpr std::array<char, 10> reserved = {{';', ',', '/', '?', ':',
                                               '@', '&', '=', '+', '$' }};

    return std::any_of(reserved.begin(), reserved.end(),
                       [&c] (const char &elem) { return elem == c; } );
}

static bool needsEscape(const char c, const bool encodeReserved)
{
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
    return false;

  if (isReservedUrlChar(c))
    return encodeReserved;

  constexpr std::array<char, 9> noNeedEscape = {{'-', '_', '.', '!', '~',
                                                '*', '\'', '(', ')' }};

  return not std::any_of(noNeedEscape.begin(), noNeedEscape.end(),
                         [&c] (const char &elem) { return elem == c; } );
}

std::string urlEncode(const std::string& value, bool encodeReserved)
{
  std::ostringstream os;
  os << std::hex << std::uppercase;
  for (std::string::const_iterator it = value.begin();
       it != value.end();
       ++it) {
    if (!needsEscape(*it, encodeReserved)) {
      os << *it;
    } else {
      os << '%' << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(*it));
    }
  }
  return os.str();
}

class ZimDumper
{
    zim::File file;
    zim::File::const_iterator pos;
    bool verbose;

  public:
    ZimDumper(const char* fname, bool titleSort)
      : file(fname),
        pos(titleSort ? file.beginByTitle() : file.begin()),
        verbose(false)
      { }

    void setVerbose(bool sw = true)  { verbose = sw; }

    void printInfo();
    void printNsInfo(char ch);
    void locateArticle(zim::size_type idx);
    void findArticle(char ns, const char* expr, bool title);
    void findArticleByUrl(const std::string& url);
    void dumpArticle();
    void printPage();
    void listArticles(bool info, bool listTable, bool extra);
    void listArticle(const zim::Article& article, bool extra);
    void listArticleT(const zim::Article& article, bool extra);
    void listArticle(bool extra)
      { listArticle(*pos, extra); }
    void listArticleT(bool extra)
      { listArticleT(*pos, extra); }
    void dumpFiles(const std::string& directory, bool symlinkdump);
    void verifyChecksum();
    void dumpFiles2One(const std::string& dumpfile, std::string input_file, const std::string& lang); /* smrz */
    std::string IntToString(int num); /* smrz */
    std::string ReplaceQuotMarks(std::string uri); /* smrz */
};

void ZimDumper::printInfo()
{
  std::cout << "count-articles: " << file.getCountArticles() << "\n";
  if (verbose)
  {
    std::string ns = file.getNamespaces();
    std::cout << "namespaces: " << ns << '\n';
    for (std::string::const_iterator it = ns.begin(); it != ns.end(); ++it)
      std::cout << "namespace " << *it << " size: " << file.getNamespaceCount(*it) << '\n';
  }
  std::cout << "uuid: " << file.getFileheader().getUuid() << "\n"
               "article count: " << file.getFileheader().getArticleCount() << "\n"
               "mime list pos: " << file.getFileheader().getMimeListPos() << "\n"
               "url ptr pos: " << file.getFileheader().getUrlPtrPos() << "\n"
               "title idx pos: " << file.getFileheader().getTitleIdxPos() << "\n"
               "cluster count: " << file.getFileheader().getClusterCount() << "\n"
               "cluster ptr pos: " << file.getFileheader().getClusterPtrPos() << "\n";
  if (file.getFileheader().hasChecksum())
    std::cout <<
               "checksum pos: " << file.getFileheader().getChecksumPos() << "\n"
               "checksum: " << file.getChecksum() << "\n";
  else
    std::cout <<
               "no checksum\n";

  if (file.getFileheader().hasMainPage())
    std::cout << "main page: " << file.getFileheader().getMainPage() << "\n";
  else
    std::cout << "main page: " << "-\n";

  if (file.getFileheader().hasLayoutPage())
    std::cout << "layout page: " << file.getFileheader().getLayoutPage() << "\n";
  else
    std::cout << "layout page: " << "-\n";

  std::cout.flush();
}

void ZimDumper::printNsInfo(char ch)
{
  std::cout << "namespace " << ch << "\n"
               "lower bound idx: " << file.getNamespaceBeginOffset(ch) << "\n"
               "upper bound idx: " << file.getNamespaceEndOffset(ch) << std::endl;
}

void ZimDumper::locateArticle(zim::size_type idx)
{
  pos = zim::File::const_iterator(&file, idx, zim::File::const_iterator::UrlIterator);
}

void ZimDumper::findArticle(char ns, const char* expr, bool title)
{
  if (title)
    pos = file.findByTitle(ns, expr);
  else
    pos = file.find(ns, expr);
}

void ZimDumper::findArticleByUrl(const std::string& url)
{
    pos = file.find(url);
}

void ZimDumper::printPage()
{
  if(pos!=file.end())
  {
    std::cout << pos->getPage() << std::flush;
  }
}

void ZimDumper::dumpArticle()
{
  if(pos!=file.end())
  {
    std::cout << pos->getData() << std::flush;
  }
}

void ZimDumper::listArticles(bool info, bool listTable, bool extra)
{
  for (zim::File::const_iterator it = pos; it != file.end(); ++it)
  {
    if (listTable)
      listArticleT(*it, extra);
    else if (info)
      listArticle(*it, extra);
    else
      std::cout << it->getUrl() << '\n';
  }
}

void ZimDumper::listArticle(const zim::Article& article, bool extra)
{
  std::cout <<
      "url: "             << article.getUrl() << "\n"
    "\ttitle:           " << article.getTitle() << "\n"
    "\tidx:             " << article.getIndex() << "\n"
    "\tnamespace:       " << article.getNamespace() << "\n"
    "\ttype:            " << (article.isRedirect()   ? "redirect"
                            : article.isLinktarget() ? "linktarget"
                            : article.isDeleted()    ? "deleted"
                            :                         "article") << "\n";

  if (article.isRedirect())
  {
    std::cout <<
      "\tredirect index:  " << article.getRedirectIndex() << "\n";
  }
  else if (article.isLinktarget())
  {
    // nothing else
  }
  else if (article.isDeleted())
  {
    // nothing else
  }
  else
  {
    std::cout <<
      "\tmime-type:       " << article.getMimeType() << "\n"
      "\tarticle size:    " << article.getArticleSize() << "\n";
  }

  if (extra)
  {
    std::string parameter = article.getParameter();
    std::cout << "\textra:           ";
    static char hexdigit[] = "0123456789abcdef";
    for (std::string::const_iterator it = parameter.begin(); it != parameter.end(); ++it)
    {
      unsigned val = static_cast<unsigned>(static_cast<unsigned char>(*it));
      std::cout << hexdigit[val >> 4] << hexdigit[val & 0xf] << ' ';
    }
    std::cout << std::endl;
  }
}

void ZimDumper::listArticleT(const zim::Article& article, bool extra)
{
  std::cout << article.getNamespace()
    << '\t' << article.getUrl()
    << '\t' << article.getTitle()
    << '\t' << article.getIndex()
    << '\t' << (article.isRedirect()   ? 'R'
              : article.isLinktarget() ? 'L'
              : article.isDeleted()    ? 'D'
              :                         'A');

  if (article.isRedirect())
  {
    std::cout << '\t' << article.getRedirectIndex();
  }
  else if (article.isLinktarget())
  {
    // nothing else
  }
  else if (article.isDeleted())
  {
    // nothing else
  }
  else
  {
    std::cout << '\t' << article.getMimeType()
              << '\t' << article.getArticleSize();
  }

  if (extra)
  {
    std::string parameter = article.getParameter();
    std::cout << '\t';
    static char hexdigit[] = "0123456789abcdef";
    for (std::string::const_iterator it = parameter.begin(); it != parameter.end(); ++it)
    {
      unsigned val = static_cast<unsigned>(static_cast<unsigned char>(*it));
      std::cout << hexdigit[val >> 4] << hexdigit[val & 0xf] << '\t';
    }
  }
  std::cout << std::endl;
}

inline void write_to_file(const std::string& path, const char* data, ssize_t size) {
#ifdef _WIN32
    auto needed_size = MultiByteToWideChar(CP_UTF8, 0, path.data(), path.size(), NULL, 0);
    std::wstring wpath(needed_size, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.data(), path.size(), &wpath[0], needed_size);
    auto fd = _wopen(wpath.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC, S_IWRITE);
#else
    auto fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif
    if (fd == -1) {
      throw std::runtime_error("Error opening file " + path);
    }
    if (write(fd, data, size) != size) {
      close(fd);
      throw std::runtime_error("Error writing to file " + path);
    }
    close(fd);
}

void ZimDumper::dumpFiles(const std::string& directory, bool symlinkdump)
{
  unsigned int truncatedFiles = 0;
#if defined(_WIN32)
  ::mkdir(directory.c_str());
#else
  ::mkdir(directory.c_str(), 0777);
#endif

  std::set<char> ns;
  for (zim::File::const_iterator it = pos; it != file.end(); ++it)
  {
    std::string d = directory + SEPARATOR + it->getNamespace();
    if (ns.find(it->getNamespace()) == ns.end())
    {
#if defined(_WIN32)
      ::mkdir(d.c_str());
#else
      ::mkdir(d.c_str(), 0777);
#endif
        ns.insert(it->getNamespace());
    }
    std::string url = it->getUrl();
    std::string::size_type p;
    while ((p = url.find('/')) != std::string::npos)
      url.replace(p, 1, "%2f");
    if ( url.length() > 255 )
    {
      std::ostringstream sspostfix, sst;
      sspostfix << (++truncatedFiles);
      sst << url.substr(0, 254-sspostfix.tellp()) << "~" << sspostfix.str();
      url = sst.str();
    }
    std::string f = d + SEPARATOR + url;
    if (it->isRedirect())
    {
        auto redirectArticle = it->getRedirectArticle();
        std::string redirectUrl = redirectArticle.getUrl();
        if (symlinkdump == false && redirectArticle.getMimeType() == "text/html")
        {
            std::ostringstream ss;
            ss <<  "<meta http-equiv=\"refresh\" content=\"0\"; url=\"";
            ss << urlEncode(redirectUrl, true);
            ss << "\" />";
            auto content = ss.str();
            write_to_file(f, content.c_str(), content.size());
        }
        else
        {
#ifdef _WIN32
            auto blob = redirectArticle.getData();
            write_to_file(f, blob.data(), blob.size());
#else
            if (symlink(redirectUrl.c_str(), f.c_str()) != 0) {
              throw std::runtime_error(
                std::string("Error creating symlink from ") + redirectUrl + " to " + f);
            }
#endif
        }
    }
    else
    {
      auto blob = it->getData();
      write_to_file(f, blob.data(), blob.size());
    }
  }
}

void ZimDumper::verifyChecksum()
{
  if (file.verify())
    std::cout << "checksum ok" << std::endl;
  else
    std::cout << "no checksum" << std::endl;
}

std::string ZimDumper::IntToString(int num){ /* smrz */
	 /* smrz */
	char s[8]; /* smrz */
	sprintf(s, "%04d", num); /* smrz */
	std::ostringstream convert; /* smrz */
	convert << s; /* smrz */
	return convert.str(); /* smrz */
} /* smrz */
 /* smrz */
std::string ZimDumper::ReplaceQuotMarks(std::string uri){ /* smrz */
 /* smrz */
	std::string new_uri = ""; /* smrz */
 /* smrz */
	for( unsigned i=0; i < uri.length(); i++ ){ /* smrz */
		if( uri.at(i) == '"' ){ /* smrz */
			new_uri += "%22"; /* smrz */
		}else{ /* smrz */
			new_uri += uri.at(i); /* smrz */
		} /* smrz */
	} /* smrz */
 /* smrz */
	return new_uri; /* smrz */
} /* smrz */
 /* smrz */
void ZimDumper::dumpFiles2One(const std::string& dump_dir, std::string input_file, const std::string& lang){ /* smrz */
 /* smrz */
	int file_no = 1; /* smrz */
	unsigned bytes_written = 0; /* smrz */
 /* smrz */
	if(::opendir(dump_dir.c_str()) == NULL){ /* smrz */
		if(::mkdir(dump_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1){ /* smrz */
			std::cout << "[Error], can't create directory"; /* smrz */
			return; /* smrz */
		} /* smrz */
	} /* smrz */
 /* smrz */
  std::size_t position = input_file.find("/"); /* smrz */
  std::regex regex_quot("\"");
  std::regex regex_newline("[\r\n]");
  while(position != std::string::npos){ /* smrz */
    input_file.erase(0, position + 1); /* smrz */
    position = input_file.find("/"); /* smrz */
  } /* smrz */
  input_file = input_file.substr(0, input_file.length()-4) + ".htmldump."; /* smrz */
 /* smrz */
	std::string file_name ( dump_dir + "/" + input_file + "part" + IntToString(file_no) ); /* smrz */
 /* smrz */
	std::ofstream out; /* smrz */
	out.open(file_name.c_str()); /* smrz */
 /* smrz */
	for( zim::File::const_iterator it = pos; it != file.end(); ++it ){ /* smrz */
 /* smrz */
		if( it->getNamespace() == 'A' ){ /* smrz */
			std::string data(it->getData().data() , it->getData().size()); /* smrz */
			std::string title, uri; /* smrz */
 /* smrz */
			std::size_t found = data.find(title_beg); /* smrz */
			// TODO: Extract URL from <link rel="canonical" href="https://en.wikipedia.org/wiki/Billboard_Music_Award_for_Top_Female_Artist">
			if( found != std::string::npos ){ /* smrz */
				title = data.substr(found + title_beg.length() , data.find(title_end) - (found + title_beg.length()) ); /* smrz */
				title.erase(title.find_last_not_of(" \t\f\v\n\r") + 1);
				title = std::regex_replace(title, regex_newline, "");
				uri = ReplaceQuotMarks(title); /* smrz */
				uri = "http://" + lang + ".wikipedia.org/wiki/" + uri; /* smrz */
				std::replace( uri.begin(), uri.end(), ' ', '_'); /* smrz */
				title = std::regex_replace(title, regex_quot, "&quot;");
			} /* smrz */
			 /* smrz */
			if( title.empty() ){ /* smrz */
				continue; /* smrz */
      } /* smrz */
 /* smrz */
			data = "<doc url=\""+ uri +"\" title=\""+ title +"\">\n" + data + "\n</doc>\n"; /* smrz */
			out << data; /* smrz */
			if( !out ){ /* smrz */
				throw std::runtime_error("error writing file " + file_name); /* smrz */
      } /* smrz */
 /* smrz */
			bytes_written += data.length(); /* smrz */
 /* smrz */
			if( bytes_written >= max_file_size ){ /* smrz */
				file_no++; /* smrz */
				bytes_written = 0; /* smrz */
				file_name = ( dump_dir + "/" + input_file + "part" + IntToString(file_no)); /* smrz */
				out.close(); /* smrz */
				out.open(file_name.c_str()); /* smrz */
			} 	 /* smrz */
		} /* smrz */
	} /* smrz */
} /* smrz */

int main(int argc, char* argv[])
{
  try
  {
    zim::Arg<bool> fileinfo(argc, argv, 'F');
    zim::Arg<char> nsinfo(argc, argv, 'N');
    zim::Arg<bool> info(argc, argv, 'i');
    zim::Arg<bool> data(argc, argv, 'd');
    zim::Arg<bool> page(argc, argv, 'p');
    zim::Arg<const char*> find(argc, argv, 'f');
    zim::Arg<const char*> url(argc, argv, 'u');
    zim::Arg<bool> list(argc, argv, 'l');
    zim::Arg<bool> tableList(argc, argv, 'L');
    zim::Arg<zim::size_type> indexOffset(argc, argv, 'o');
    zim::Arg<bool> extra(argc, argv, 'x');
    zim::Arg<char> ns(argc, argv, 'n', 'A');  // namespace
    zim::Arg<const char*> dumpAll(argc, argv, 'D');
    zim::Arg<bool> verbose(argc, argv, 'v');
    zim::Arg<bool> titleSort(argc, argv, 't');
    zim::Arg<bool> verifyChecksum(argc, argv, 'C');
    zim::Arg<const char*> lang(argc, argv, 'J'); /* smrz */
    zim::Arg<const char*> dumpAll2One(argc, argv, 'a'); /* smrz */
    zim::Arg<bool> printVersion(argc, argv, 'V');
#ifndef _WIN32
    zim::Arg<bool> redirectSymlink(argc, argv, 's');
#endif

    // version number
    if (printVersion)
    {
      version();
      return 0;
    }

    if (argc <= 1)
    {
      std::cerr << "usage: " << argv[0] << " [options] zimfile\n"
                   "\n"
                   "options:\n"
                   "  -F        print fileinfo\n"
                   "  -N ns     print info about namespace\n"
                   "  -i        print info about articles\n"
                   "  -d        print data of articles\n"
                   "  -p        print page\n"
                   "  -f title  find article\n"
                   "  -u url    find article by url\n"
                   "  -t        sort (and find) articles by title instead of url\n"
                   "  -l        list articles\n"
                   "  -L        list articles as table\n"
                   "  -o idx    locate article by index\n"
                   "  -x        print extra parameters\n"
                   "  -n ns     specify namespace (default 'A')\n"
                   "  -D dir    dump all files into directory\n"
#ifndef _WIN32
                   "  -s        Use symlink to dump html redirect. Else create html redirect file."
#endif
                   "  -v        verbose (print uncompressed length of articles when -i is set)\n"
                   "                    (print namespaces with counts with -F)\n"
                   "  -V        print the software version number\n"
                   "  -Z        dump index data\n"
                   "  -C        verify checksum\n"
                   "  -a dumpf  dump all namespace A files into dumpf\n" /* smrz */
                   "  -J lang   set language for the links\n" /* smrz */
                   "\n"
                   "examples:\n"
                   "  " << argv[0] << " -F wikipedia.zim\n"
                   "  " << argv[0] << " -l wikipedia.zim\n"
                   "  " << argv[0] << " -f Auto -i wikipedia.zim\n"
                   "  " << argv[0] << " -f Auto -d wikipedia.zim\n"
                   "  " << argv[0] << " -f Auto -l wikipedia.zim\n"
                   "  " << argv[0] << " -f Auto -l -i -v wikipedia.zim\n"
                   "  " << argv[0] << " -o 123159 -l -i wikipedia.zim\n"
                   "  " << argv[0] << " -a html_from_wikipedia_en_all_novid_2018-10.zim/ -J en wikipedia.zim\n" /* smrz */
                 << std::flush;
      return -1;
    }

    // initalize app
    ZimDumper app(argv[1], titleSort);
    app.setVerbose(verbose);

    // global info
    if (fileinfo)
      app.printInfo();

    // namespace info
    if (nsinfo.isSet())
      app.printNsInfo(nsinfo);

    // locate article
    if (indexOffset.isSet())
      app.locateArticle(indexOffset);
    else if (find.isSet())
      app.findArticle(ns, find, titleSort);
    else if (url.isSet())
      app.findArticleByUrl(std::string(url));

    // dump files
    if (dumpAll.isSet()) {
#ifdef _WIN32
      app.dumpFiles(dumpAll.getValue(), false);
#else
      app.dumpFiles(dumpAll.getValue(), redirectSymlink);
#endif
    }

    // dump files to one /* smrz */
    if (dumpAll2One.isSet()){ /* smrz */
        if(lang.isSet()){ /* smrz */
                app.dumpFiles2One(dumpAll2One.getValue(), argv[1], lang.getValue()); /* smrz */
        } /* smrz */
        else{ /* smrz */
                app.dumpFiles2One(dumpAll2One.getValue(), argv[1], "en"); /* smrz */
        } /* smrz */
    } /* smrz */

    // print requested info
    if (data)
      app.dumpArticle();
    else if (page)
      app.printPage();
    else if (list || tableList)
      app.listArticles(info, tableList, extra);
    else if (info)
      app.listArticle(extra);

    if (verifyChecksum)
      app.verifyChecksum();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return -2;
  }
  return 0;
}

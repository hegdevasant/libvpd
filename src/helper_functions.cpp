/***************************************************************************
 *   Copyright (C) 2006, IBM                                               *
 *                                                                         *
 *   Maintained By:                                                        *
 *   Eric Munson and Brad Peters                                           *
 *   munsone@us.ibm.com, bpeters@us.ibm.com                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the Lesser GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or at your option) any later version.                        *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Lesser General Public License for more details.                   *
 *                                                                         *
 *   You should have received a copy of the Lesser GNU General Public      *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <libvpd-2/helper_functions.hpp>
#include <libvpd-2/logger.hpp>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sstream>
#include <sys/wait.h>
#include <linux/limits.h>
#include <libgen.h>


#define BUF_SIZE PATH_MAX

using namespace std;
using namespace lsvpd;

static int process_child(char *argv[], int pipefd[])
{
	int     nullfd;

	close(pipefd[0]);

	/* stderr to /dev/null redirection */
	nullfd = open("/dev/null", O_WRONLY);
	if (nullfd == -1) {
		log_notice("Failed to open \'/dev/null\' for redirection "
			   "(%d:%s).", errno, strerror(errno));
		close(pipefd[1]);
		return -1;
	}

	/* redirect stdout to write-end of the pipe */
	if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
		log_notice("Failed to redirect pipe write fd to stdout "
			   "(%d:%s).", errno, strerror(errno));
		goto err_out;
	}

	if (dup2(nullfd, STDERR_FILENO) == -1) {
		log_notice("Failed to redirect \'/dev/null\' to stderr "
			   "(%d:%s).", errno, strerror(errno));
		goto err_out;
	}

	execve(argv[0], argv, NULL);
	/* some failure in exec */

err_out:
	close(pipefd[1]);
	close(nullfd);
	return -1;
}

/*
 * This function mimics popen(3).
 *
 * Returns:
 *   NULL, if fork(2), pipe(2) and dup2(3) calls fail
 *
 * Note:
 *   fclose(3) function shouldn't be used to close the stream
 *   returned here, since it doesn't wait for the child to exit.
 */
FILE *spopen(char *argv[], pid_t *ppid)
{
	FILE    *fp = NULL;
	int     pipefd[2];
	pid_t   cpid;

	if (argv == NULL)
		return fp;

	if (access(argv[0], F_OK|X_OK) != 0) {
		log_notice("The command \"%s\" is not executable.", argv[0]);
		return fp;
	}

	if (pipe(pipefd) == -1) {
		log_notice("Failed in pipe(), error: %d:%s",
			   errno, strerror(errno));
		return NULL;
	}

	cpid = fork();
	switch (cpid) {
	case -1:
		/* Still in parent; Failure in fork() */
		log_notice("fork() failed, error : %d:%s",
			   errno, strerror(errno));
		close(pipefd[0]);
		close(pipefd[1]);
		return NULL;
	case  0: /* Code executed by child */
		if (process_child(argv, pipefd) == -1) {
			log_notice("Error occured while processing write end "
				   "of the pipe (in child).");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	default: /* Code executed by parent */
		/* store the child pid for pclose() */
		*ppid = cpid;

		close(pipefd[1]);

		fp = fdopen(pipefd[0], "r");
		if (fp == NULL) {
			log_notice("fdopen() error : %d:%s",
				   errno, strerror(errno));
			close(pipefd[0]);
			return NULL;
		}
		break;
	}

	return fp;
}

/*
 * This function closely mimics pclose(3).
 * Returns :
 *   On success :  exit status of the command as returned by waitpid(2),
 *   On failure : -1 if waitpid(2) returns an error.
 *   If it cannot obtain the child status, errno is set to ECHILD.
 */
int spclose(FILE *stream, pid_t cpid)
{
	int     status;
	pid_t   pid;

	/*
	 * Close the stream, fclose() takes care of closing
	 * the underlying fd.
	 */
	if (fclose(stream) == EOF) {
		log_notice("Failed in fclose() (%d:%s)",
			   errno, strerror(errno));
		return -1;
	}

	/* Wait for the child to exit */
	do {
		pid = waitpid(cpid, &status, 0);
	} while (pid == -1 && errno == EINTR);

	/* Couldn't obtain child status */
	if (status == -1)
		errno = SIGCHLD;

	return status;
}

	/**
	 * findAIXFSEntry
	 * @brief Finds AIX name which exists as a file or link in the dir
	 *	'rootPath'
	 */
	string HelperFunctions::findAIXFSEntry(vector <DataItem*> aixNames,
											const string& rootPath)
	{
		string fin;
		int fd;
		vector<DataItem*>::const_iterator i, end;
		i = aixNames.begin();
		end = aixNames.end();

		while (i != end) {
			fin = rootPath + (*i)->getValue();
			fd = open( fin.c_str( ), O_RDONLY | O_NONBLOCK );
			if( fd < 0 )
				i++;
			else {
				close(fd);
				return fin;
			}
		}
		return string("");
	}

/**
 * parseString
 * @brief Parses out strings based on a count of position [0...x],
 *  with beginning and end of each string given by a '"'.
 * Ex: "string 0" ... "string 1" ... "string 2" ...
 * If position is less/more than the string available, then it logs
 * an error and returns empty string.
 */
string HelperFunctions::parseString(const string& line, int str_pos, string& out)
{
        size_t pos = 0, beg = 0;
	int str_pos_prov = str_pos;

	if (line == "")
		return string();

        if (str_pos <= 0) {
		log_info("Invalid position : %d", str_pos);
                return string();
        }

        while (str_pos > 0) {
                beg = line.find('"', pos);
                if (beg == string::npos) {
			log_info("String not found at position: %d",
				 (str_pos_prov - str_pos) + 1);
                        return string();
                }

                pos = line.find('"', beg + 1);
                if (pos == string::npos) {
			log_info("String at position %d not terminated properly",
				 (str_pos_prov - str_pos) + 1);
                        return string();
                }
                pos++;
                str_pos--;
	}

        out = line.substr(beg + 1, pos - beg - 2);
        return out;
}

	/**
	 * parsePathr
	 * @brief Reverse parse of a path, returning the node specified by 'count'
	 * @param count 0 is the tail node, 1 is 1 in, etc. 
	 * @example path = /sys/devices/x/y/z, 
	 * @ex count = 0, returns z
	 *  count = 1, returns y
	 *  count = 2, returns x
	 */
	string HelperFunctions::parsePathr(const string& path, int count)
	{
		int beg, end;
		int i = count;

		if( path == "" )
			return path;

		beg = end = path.length();

		if (path[end] == '/')
			beg--;

		cout << "String: " << path << endl;

		while (i >= 0) {
			end = beg - 1;
			cout << "RFind Bef: beg: " << beg << ", end: " << end << ", i: " << i << endl;
			beg = path.rfind("/", end - 1) + 1;
			cout << "RFind : beg: " << beg << ", end: " << end << ", i: " << i << endl;
			i--;
		}

		return path.substr(beg, end - beg);
	}

	/**
	 * parsePath
	 * @brief Parse of a path, returning the node specified by 'count'
	 * @param count 0 is the first node, 1 is 1 in, etc. 
	 * @example path = /sys/devices/x/y/z, 
	 * @ex count = 0, returns sys
	 *  count = 1, returns devices
	 *  count = 2, returns x
	 */
	string HelperFunctions::parsePath(const string& path, int count)
	{
		int beg = 0, end = 0, i = count;

		if( path == "" )
			return path;

		if (path[end] == '/')
			end--;

		while (i >= 0) {
			beg = path.find("/", beg) + 1;
			i--;
		}

		end = path.find("/", beg);

		return path.substr(beg, end - beg);
	}

	/**
	 * getSymLinkTarget
	 * @brief Returns the absolute path of regular file pointed at by a sym link
	 * @param full path of sym link
	 * @return String abs target
	 */
	string HelperFunctions::getSymLinkTarget(const string& symLinkPath)
	{
		char linkTarget[PATH_MAX];

		if ( realpath( symLinkPath.c_str(), linkTarget ) == NULL )
			return string();

		return string( linkTarget );
	}

/* Drops one dir entry on the end of a path
	 * @return Returns number of dirs successfully dropped*/
	int HelperFunctions::dropDir(char * filePath)
	{
		int len = strlen(filePath);
		int i, ret = 0;

		if (len <= 0)
			return 0;

		/* erase filenames */
		i = len;
		while (i > 0 && filePath[i] != '/')
			filePath[i--] = '\0';

		filePath[len-1] = '\0'; //erase trailing '/'

		i = len;
		while (i > 0 && filePath[i] != '/') {
			filePath[i--] = '\0';
			ret = 1;
		}

		return ret;
	}

	/* Drops one dir entry on the end of a path
	 * @return Returns number of dirs successfully dropped*/
	int HelperFunctions::dropDir(string& filePath_t)
	{
		int len = filePath_t.length();
		int i, ret = 0;
		char *filePath;

		if (len <= 0)
			return 0;

		filePath = strdup(filePath_t.c_str());
		if (filePath == NULL)
			return 0;

		/* erase filenames */
		i = len;
		while (i > 0 && filePath[i] != '/')
			filePath[i--] = '\0';

		filePath[len-1] = '\0'; //erase trailing '/'

		i = len;
		while (i > 0 && filePath[i] != '/') {
			filePath[i--] = '\0';
			ret = 1;
		}
		filePath[i--] = '\0';

		filePath_t = string(filePath);
		free(filePath);
		return ret;
	}

	/* getAbsolutePath
	 * @brief It returns the absolute Path of Destination string(file/Dir)
	 * where symlink points to.
	 * @param relative path of destination directory w.r.t curDir
	 * @param Absolute path of symlink(file/folder)
	 * @return Absolute path of Destination Directory(i.e relPath), where
	 * symlink(curDir) points to.
	 */
	string HelperFunctions::getAbsolutePath(char * relPath, char * curDir)
	{
		/* Converting it to string class object, to escape NULL checking */
		string relPath_s(relPath), curDir_s(curDir);

		/* If string is empty */
		if ( relPath_s.empty() || curDir_s.empty() )
			return string();

		/* If relPath_s contains absolute path, then return relPath_s */
		if ( relPath_s[0] == '/' )
			return relPath_s;

		curDir_s =  dirname((char *)curDir_s.c_str());

		string rpath = curDir_s + '/' + relPath_s;
		char linkTarget[PATH_MAX];

		if ( realpath( rpath.c_str(), linkTarget ) == NULL )
			return string();

		return string( linkTarget );
	}

	/**
	 * Takes two strings and finds if the two are equivalent.
	 * s1 can contain '*', meaning zero or more of anything can be
	 * counted as a match
	 */
	bool HelperFunctions::matches(const string& s1, const string& s2)
	{
		size_t beg = 0, end;
		size_t z;
//		coutd << " s1 = " << s1 << ", s2 = " << s2 << endl;

		//strings have matched to end - base case
		if ((s1 == s2) || (s1.length() == 0) || (s2.length() == 0)) {
			return (s1 == s2);
		}

		//s1 has matched all the way up to a * - base case
		if (s1 == "*") {
			return true;
		}

		if (s1[0] == '*') {
			/*
			 * Need to grab a substring of s2, up to size of s1 from
			 * star to any other star
			 */
			beg = 1;
			end = s1.length();

			if ( string::npos == (end = s1.find("*", beg))) {
				//No more stars - base case
				if (string::npos != (z = s2.find(s1.substr(beg, end), 0))) {
					return true;
				}
			}
			else {
				if (string::npos != s2.find(s1.substr(beg, end), 0))
					matches(s1.substr(end, s1.length() - end),
							s2.substr(end, s2.length() - end));
			}
		}
		else
			end = s1.find("*", 0);

		if (s1.substr(beg, end) == s2.substr(beg, end)) {
			return matches(s1.substr(end, s1.length() - end),
						s2.substr(end, s2.length() - end));
		}
		else {
			return false;
		}
	}

	/* Caller responsible for deleting new'd memory */
	void HelperFunctions::str2chr(char **str1, const string& str2)
		{
			unsigned int i;
			*str1 = new char[ str2.length( ) * sizeof( char ) + 1 ];
			if ( *str1 != NULL ) {
				i=0;
				while (i < str2.length())
				{
					(*str1)[i] = str2[i];
					i++;
				}

				(*str1)[i] = '\0';
			}
		}

	/* @brief Ensure a passed in fs path arg is in reasonable format */
	void HelperFunctions::fs_fixPath(string& str)
	{
		int i = str.length();

		while (str[i] == '\0')
			i--;
		if (str[i] == '/')
			str[i] = '\0';
	}

	/* @brief Ensure a passed in fs path arg is in reasonable format
	 * It mainly checks for '/' in the end.
	 * @param e.g: /proc/device-tree
	 */
	char *HelperFunctions::fs_fixPath(char *path_t)
	{
		char *path;
		char *chr = NULL;

		if ( path_t == NULL )
			return NULL;

		path = strdup(path_t);
		if ( path == NULL )
			return NULL;

		/* Basic weirdness checking */
		chr = &path[strlen(path) - 1];
		if ( *chr == '/' )
			*chr = '\0';

		return path;
	}

	bool HelperFunctions::file_exists(const string& file)
	{
		/* If file is available and have read permission */
		if (access(file.c_str(), R_OK) == 0)
			return true;

		/* If file is not present - caller will create it, so no need
		 * to log error. But if file is present but read access call
		 * gets failed due to other errors condition except(ENOENT),
		 * then error should be logged.
		 */
		if (errno != ENOENT)
			log_err("Failed to access file %s : %s", file.c_str(),
				strerror(errno));

		return false;
	}

/** readMatchFromFile
 *  @brief Returns first matching line from specified file,
 *    in which was found a match to the specified string
*/
string HelperFunctions::readMatchFromFile(const string& file, const string& str)
{
	char buf[BUF_SIZE];
	string line;

	if (file_exists(file)) {
		ifstream fin(file.c_str());

		while(!fin.eof()) {
			fin.getline(buf,BUF_SIZE - 1);
			line = string(buf);
			int loc = line.find(str);
			if (loc == 0) {
				fin.close();
				return line;
			}
		}
		fin.close();
		return string("");
	}
	return string("");
}


	int HelperFunctions::countChar( const string& str, char c )
	{
		string::const_iterator i, end;
		int ret = 0;
		for( i = str.begin( ), end = str.end( ); i != end; ++i )
		{
			if( (*i) == c )
			{
				ret++;
			}
		}
		return ret;
	}

	bool HelperFunctions::contains( const vector<DataItem*>& vec,
		const string& val )
	{
		vector<DataItem*>::const_iterator i, end;
		for( i = vec.begin( ), end = vec.end( ); i != end; ++i )
		{
			if( (*i)->getValue( ) == val )
				return true;
		}

		return false;
	}

	int HelperFunctions::execCmd( const char *cmd, string& output )
	{
		char buf[BUF_SIZE];
		char *system_args[32] = {NULL,};
		pid_t cpid;
		int i = 0;
		int rc = -1;
		istringstream ss(cmd);
		string s;
		FILE *fp;

		while ( ss >> s ) {
			if (i < 32) {
				system_args[i] = strdup(s.c_str());
				if (system_args[i] == NULL)
					goto free_mem;
				i++;
			} else {
				goto free_mem;
			}
		}

		fp = spopen(system_args, &cpid);
		if (fp == NULL)
			goto free_mem;

		while ( !feof(fp) )
			if (fgets(buf, BUF_SIZE - 1, fp) != NULL)
				output += buf;

		spclose(fp, cpid);
		rc = 0;

free_mem:
		for (i -= 1; i >= 0; i--)
			free(system_args[i]);

		return rc;
	}

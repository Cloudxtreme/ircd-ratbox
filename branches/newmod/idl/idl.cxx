/*
 * Copyright 2004 Edward Brocklesby.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, 
 * copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER  LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * $Id$
 */

#include <idl/stdinc.hxx>
#include <idl/parser.hxx>

void usage		(string const & progname);
void process	(string const & file);

int main (int argc, char *argv[])
{
	if (argc < 2)
		usage(argv[0]);
		
	while (*++argv) {
		process(*argv);
	}
}

void usage (string const & name)
{
	cerr << format("Usage: %s [options] file [file ...]\n") % name;
	exit(EXIT_FAILURE);
}

void process (string const & file)
{
	idl::result r;
	idl::parser p(file);
	p.parse(r);
	if (p) {
		cerr << "parsed ok, found interfaces:\n";
		for (idl::result::interface_iterator it = r.begin(), end = r.end();
			 it != end; ++it) {
			cerr << it->name();
		}
		cerr << "\n";	 
	} else {
		cerr << "failed parse\n";
	}
}



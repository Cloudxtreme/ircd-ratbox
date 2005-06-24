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

#include <exception>
#include <iostream>
#include <fstream>

#include <boost/spirit.hpp>
#include <boost/spirit/iterator.hpp>
#include <boost/spirit/iterator/file_iterator.hpp>
#include <boost/spirit/core/scanner/skipper.hpp>
#include <boost/format.hpp>

#include <idl/stdinc.hxx>
#include <idl/parser.hxx>

using namespace boost;
using namespace boost::io;
using namespace boost::spirit;

namespace idl { namespace impl {

class idlp {
public:
	typedef position_iterator<file_iterator<> > pos_iterator_t;

	bool	parse	(std::string const & file, result& r);
	
private:
	file_position	filepos;
};

} // namespace impl 

parser::parser (std::string const & file_)
: file	(file_)
, state	(false)
, impl	(0)
{
	cerr << "parser for " << file << "\n";
}

bool parser::parse (result& r)
{
	impl = new impl::idlp;
	return state = impl->parse(file, r);
}

parser::operator bool (void)
{
	return state;
}

namespace impl {

struct idl_grammar : public grammar<idl_grammar>
{
	idl::result& r;
	
	idl_grammar (idl::result& r_)
	: r(r_)
	{ 
	}

	struct skip_parser : public grammar<skip_parser>
	{
		template<typename ScannerT>
		struct definition
		{
			definition(skip_parser const& self)
			{
				comment
					=	comment_p("/*", "*/")
					|	comment_p("//")
					|	space_p
					;
				
				BOOST_SPIRIT_DEBUG_RULE(comment);
			}
			rule<ScannerT> comment;
			rule<ScannerT> const & start() const { return comment; }
		};
	};
	
	template<typename ScannerT>
	struct definition
	{
		definition(idl_grammar const& self)
		{
			keywords
				=	"interface", "extends", "implementation-language",
					"implemented-by"
				;
				
			begin
				=	*stmts
				;
			
			stmts
				=	interface_stmt
				;
			
			interface_stmt
				=	"interface" 
					>> identifier [result::open_interface(self.r)]
					>> !extends_specifier
					>> '{' 
					>> *interface_stmts
					>> ch_p('}') [result::close_interface(self.r)] 
					>> ';' 
				;
				
			extends_specifier
				=	"extends"
					>> identifier
				;
				
			interface_stmts
				=	function_stmt
				|	implementation_language_stmt
				;
			
			function_stmt
				=	"function"
					>> identifier [result::open_function(self.r)]
					>> ':'
					>> '('
					/* parameter list */
					>> (	ch_p('(') >> ch_p(')')
					   |	(	'('
					    		>>	(	type [result::push_function_parameter(self.r)]
										% ","
									)
								>> ')'
							)
					   )
					/* return type */	
					>> "->"
					>> (	ch_p('(') >> ch_p(')')
					   |	type [result::push_function_return_type(self.r)]	
					   )
					>> ')'
					>> !(
							"implemented-by" 
							>> identifier [result::implemented_by(self.r)]
						)
					>> ch_p(';') [result::close_function(self.r)]
				;
			
			implementation_language_stmt
				=	str_p("implementation-language")
					>> quoted_string [result::set_implementation_language(self.r)]
					>> ';'
				;
			
			type
				=	str_p("bool") | "int" | "string"
				;
						
			identifier
				=	lexeme_d[
						((alpha_p | '_') >> *(alnum_p | '_'))
						- (keywords >> anychar_p - (alnum_p | '_'))
					]
				;
			
			alpha
				=	range_p('A', 'Z')
				|	range_p('a', 'z')
				;
			
			alnum
				=	alpha
				|	range_p('0', '9')
				;
			
			quoted_string
				=	lexeme_d[
						ch_p('\"')
						>> *(
								anychar_p - ch_p('\"')
							)
						>> ch_p('\"')
					]
				;
			
			BOOST_SPIRIT_DEBUG_RULE(stmts);
			BOOST_SPIRIT_DEBUG_RULE(interface_stmts);
			BOOST_SPIRIT_DEBUG_RULE(interface_stmt);
			BOOST_SPIRIT_DEBUG_RULE(extends_specifier);
			BOOST_SPIRIT_DEBUG_RULE(alpha);
			BOOST_SPIRIT_DEBUG_RULE(alnum);
			BOOST_SPIRIT_DEBUG_RULE(quoted_string);
			BOOST_SPIRIT_DEBUG_RULE(identifier);
			BOOST_SPIRIT_DEBUG_RULE(type);
			BOOST_SPIRIT_DEBUG_RULE(function_stmt);
			BOOST_SPIRIT_DEBUG_RULE(implementation_language_stmt);
			BOOST_SPIRIT_DEBUG_RULE(begin);
		}
		
		symbols<>		keywords;
		
		rule<ScannerT> begin;
		rule<ScannerT> stmts, interface_stmts;
		rule<ScannerT> interface_stmt;
		rule<ScannerT> extends_specifier;
		rule<ScannerT> alpha, alnum, quoted_string;
		rule<ScannerT> identifier, type;
		rule<ScannerT> function_stmt, implementation_language_stmt;
		rule<ScannerT> comment;
		rule<ScannerT> const & start() const { return begin; }
		rule<ScannerT> const & comment_rule() const { return comment; }
	};
};
	
bool impl::idlp::parse (std::string const & file, result& r)
{
	file_iterator<> first(file.c_str()), last = first.make_end();
	filepos.file = file;
	pos_iterator_t begin(first, last, filepos), end;
	parse_info<pos_iterator_t> info;

	idl_grammar g(r);
	idl_grammar::skip_parser skip_p;

	info = spirit::parse(begin, end, g, skip_p);
	
	if (info.full) {
		return true;
	} else {
		return false;
	}
}

} // namespace impl

} // namespace idl

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

#ifndef IDL_PARSER_HXX
#define IDL_PARSER_HXX

#include <string>
#include <vector>
#include <list>

namespace idl {
namespace impl {
	class idlp;
}

class type {
public:
	enum basictype {
		t_char,
		t_integer,
		t_string,
		t_array
	};

private:
	basictype m_type;
};

class function_argument {
public:
	function_argument (type::type type_)
	: m_type(type_)
	{ }

private:
	type::type m_type;
};

class function {
public:
	function (std::string const & name_)
	: m_name(name_)
	{ }
	
	std::string const & name (void) const {
		return m_name;
	}

	void set_return_type (std::string const & type) {
		/* XXX implement */
	}
	
	void add_parameter (std::string const & type) {
		/* XXX implement */
	}
	
private:
	std::string							m_name;
	std::list<idl::function_argument> 	m_arguments;
	idl::type							m_return_type;
};

class interface {
public:
	interface (std::string const & name_)
	: m_name(name_)
	{ }
	
	std::string const &	name (void) const {
		return m_name;
	}

	void push_function (idl::function& func) {
		functions.push_back(idl::function(func));
	}
	
	void set_implementation_language (std::string const & lang) {
		m_impl_lang = lang;
	}
	
	idl::function *curr_function;
	
private:
	std::string					 m_name;
	std::vector<idl::function>	 functions;
	std::string					 m_impl_lang;
};

class result {
public:
	void push_interface(interface const & i)
	{ interfaces.push_back(i); }

/* Callback functions from the parser
 * XXX should these be somewhere else?
 */
	struct open_interface {
		open_interface (idl::result& r_)
		: r(r_) { }
		
		template<class IteratorT>
		void operator() (IteratorT first, IteratorT last) const {
			r.curr_interface = new idl::interface (std::string(first, last));
		}
		idl::result& r;
	};
	
	struct close_interface {
		close_interface (idl::result& r_)
		: r(r_) { }
		
		void operator() (char) const {
			r.push_interface(*r.curr_interface);
			delete r.curr_interface;
		}
		idl::result& r;
	};
	
	struct set_implementation_language {
		set_implementation_language (idl::result& r_) : r(r_) { }
		
		template<class IteratorT>
		void operator() (IteratorT first, IteratorT last) const {
			r.curr_interface->set_implementation_language(std::string(first, last));
		}
		idl::result& r;
	};
	
	struct open_function {
		open_function (idl::result& r_)
		: r(r_) { }
		
		template<class IteratorT>
		void operator() (IteratorT first, IteratorT last) const {
			r.curr_interface->curr_function = 
				new idl::function(std::string(first, last));
		}
		idl::result& r;
	};
	
	struct close_function {
		close_function (idl::result& r_)
		: r(r_) { }
		
		void operator() (char) const {
			r.curr_interface->push_function(*r.curr_interface->curr_function);
			delete r.curr_interface->curr_function;
		}
		idl::result& r;
	};
	
	struct push_function_parameter {
		push_function_parameter (idl::result& r_)
		: r(r_) { }
		
		template<class IteratorT>
		void operator() (IteratorT first, IteratorT last) const {
			r.curr_interface->curr_function->add_parameter
				(std::string(first, last));
		}
		idl::result& r;
	};
	
	struct push_function_return_type {
		push_function_return_type (idl::result& r_)
		: r(r_) { }
		
		template<class IteratorT>
		void operator() (IteratorT first, IteratorT last) const {
			r.curr_interface->curr_function->set_return_type
				(std::string(first, last));
		}
		idl::result& r;
	};
	
	struct implemented_by {
		implemented_by (idl::result& r_) : r(r_) { }
		template<class IteratorT>
		void operator() (IteratorT, IteratorT) const {
		}
		idl::result& r;
	};
	
		
	typedef std::vector<interface>::iterator interface_iterator;
	typedef std::vector<interface>::const_iterator const_interface_iterator;
	const_interface_iterator begin() const { return interfaces.begin(); }
	const_interface_iterator end() const { return interfaces.end(); }
	interface_iterator begin() { return interfaces.begin(); }
	interface_iterator end() { return interfaces.end(); }

	interface_iterator last() { return interfaces.end() - 1; }
	
	idl::interface*			curr_interface;
	
private:
	std::vector<interface>	interfaces;
};
	
class parser {
public:
			parser 		(std::string const &);
	
	bool	parse			(result &);
	void	print_error		(void);

	operator bool (void);
	
private:
	std::string		 file;
	bool			 state;
	impl::idlp 		*impl;
};

}

#endif

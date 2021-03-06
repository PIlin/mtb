/*============================================================================
  HDRITools - High Dynamic Range Image Tools
  Copyright 2008-2011 Program of Computer Graphics, Cornell University

  Distributed under the OSI-approved MIT License (the "License");
  see accompanying file LICENSE for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
 ----------------------------------------------------------------------------- 
 Primary author:
     Edgar Velazquez-Armendariz <cs#cornell#edu - eva5>
============================================================================*/

#pragma once
#if !defined(PCG_EXCEPTION_H)
#define PCG_EXCEPTION_H

#include "ImageIO.h"
#include <string>

// Macro to save typing when subclassing the basic exception
// based on the IexBaseExc.h macro
#define PCG_DEFINE_EXC(name, base)				            \
    class name: public base				                    \
    {							                            \
      public:                                               \
	name (const char* text=0)      throw(): base (text) {}	\
	name (const std::string &text) throw(): base (text) {}	\
	virtual ~name() throw() {} \
    };

namespace pcg {

	// Basic exception type
	class Exception: public std::exception
	{
	  public:
		Exception (const char* text=0) throw()     : message(text) {}
		Exception (const std::string &text) throw(): message(text) {}

		virtual ~Exception() throw () {}

		virtual const char * what () const throw () {
			return message.c_str();
		}

	private:
		std::string message;
	};

	// Some specific exceptions
	PCG_DEFINE_EXC(IllegalArgumentException, Exception)
	PCG_DEFINE_EXC(IOException, Exception)
	PCG_DEFINE_EXC(RuntimeException, Exception)

}

#endif /* PCG_EXCEPTION_H */

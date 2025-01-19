/*
 * opencog/atoms/opencl/OpenclStream.cc
 *
 * Copyright (C) 2025 Linas Vepstas
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <fstream>

#include <opencog/util/exceptions.h>
#include <opencog/util/Logger.h>
#include <opencog/util/oc_assert.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Link.h>
#include <opencog/atoms/base/Node.h>
#include <opencog/atoms/core/NumberNode.h>
#include <opencog/atoms/value/StringValue.h>
#include <opencog/atoms/value/ValueFactory.h>

#include <opencog/opencl/types/atom_types.h>
#include <opencog/sensory/types/atom_types.h>
#include "OpenclStream.h"

using namespace opencog;

OpenclStream::OpenclStream(const std::string& str)
	: OutputStream(OPENCL_STREAM)
{
	init(str);
}

OpenclStream::OpenclStream(const Handle& senso)
	: OutputStream(OPENCL_STREAM)
{
	if (SENSORY_NODE != senso->get_type())
		throw RuntimeException(TRACE_INFO,
			"Expecting SensoryNode, got %s\n", senso->to_string().c_str());

	init(senso->get_name());
}

OpenclStream::~OpenclStream()
{
	// Runs only if GC runs. This is a problem.
	halt();
}

void OpenclStream::halt(void) const
{
	_value.clear();
}

// ==============================================================

void OpenclStream::find_device(void)
{
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);

	for (const auto& plat : platforms)
	{
		std::string pname = plat.getInfo<CL_PLATFORM_NAME>();
		if (0 < _splat.size() and pname.find(_splat) == std::string::npos)
			continue;

		std::vector<cl::Device> devices;
		plat.getDevices(CL_DEVICE_TYPE_ALL, &devices);
		for (const cl::Device& dev: devices)
		{
			std::string dname = dev.getInfo<CL_DEVICE_NAME>();
			if (dname.find(_sdev) == std::string::npos)
				continue;

			_platform = plat;
			_device = dev;

			logger().info("OpenclStream: Using platform '%s' and device '%s'\n",
				pname.c_str(), dname.c_str());

			return;
		}
	}

	throw RuntimeException(TRACE_INFO,
		"Unable to find platform:device in URL \"%s\"\n",
		_uri.c_str());
}

// ==============================================================

void OpenclStream::build_kernel(void)
{
	// Copy in source code. Must be a better way!?
	std::ifstream srcfm(_filepath);
	std::string src(std::istreambuf_iterator<char>(srcfm),
		(std::istreambuf_iterator<char>()));

	if (0 == src.size())
		throw RuntimeException(TRACE_INFO,
			"Unable to find source file in URL \"%s\"\n",
			_uri.c_str());

	cl::Program::Sources sources;
	sources.push_back(src);

	_program = cl::Program(_context, sources);

	// Compile
	try
	{
		// Specifying flags causes exception.
		// program.build("-cl-std=CL1.2");
		_program.build("");
	}
	catch (const cl::Error& e)
	{
		logger().info("OpenclStream failed compile >>%s<<\n",
			_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(_device).c_str());
		throw RuntimeException(TRACE_INFO,
			"Unable to compile source file in URL \"%s\"\n",
				_uri.c_str());
	}
}

// ==============================================================

void OpenclStream::load_kernel(void)
{
	// Copy in SPV file. Must be a better way!?
	std::ifstream spvfm(_filepath);
	std::string spv(std::istreambuf_iterator<char>(spvfm),
		(std::istreambuf_iterator<char>()));

	if (0 == spv.size())
		throw RuntimeException(TRACE_INFO,
			"Unable to find SPV file in URL \"%s\"\n",
			_uri.c_str());

	_program = cl::Program(_context, spv);
}

// ==============================================================

#define BAD_URL \
	throw RuntimeException(TRACE_INFO, \
		"Unsupported URL \"%s\"\n" \
		"\tExpecting 'opencl://platform:device/file/path/kernel.cl'", \
		_uri.c_str());

/// Attempt to open connection to OpenCL device
void OpenclStream::init(const std::string& url)
{
	do_describe();
	if (0 != url.compare(0, 9, "opencl://")) BAD_URL;

	// Make a copy, for debuggingg purposes.
	_uri = url;

	// Ignore the first 9 chars "opencl://"
	size_t pos = 9;

	// Extract platform name substring
	size_t platend = _uri.find(':', pos);
	if (std::string::npos == platend) BAD_URL;
	if (pos < platend)
	{
		_splat = _uri.substr(pos, platend-pos);
		pos = platend;
	}
	pos ++;

	// Extract device name substring
	size_t devend = _uri.find('/', pos);
	if (std::string::npos == devend) BAD_URL;
	if (pos < devend)
	{
		_sdev = _uri.substr(pos, devend-pos);
		pos = devend;
	}
	_filepath = _uri.substr(pos);

	// Try to create the OpenCL device
	find_device();
	_context = cl::Context(_device);
	_queue = cl::CommandQueue(_context, _device);

	// Tryt ot load source or spv file
	pos = _uri.find_last_of('.');
	if (std::string::npos == pos) BAD_URL;

	if (_uri.substr(pos) == ".spv")
		load_kernel();
	else
		build_kernel();
}

// ==============================================================

Handle _global_desc = Handle::UNDEFINED;

void OpenclStream::do_describe(void)
{
	if (_global_desc) return;

	HandleSeq cmds;

	// Describe exactly how to Open this stream.
	// It needs no special arguments.
	Handle open_cmd =
		make_description("Open connection to GPU",
		                 "OpenLink", "OpenclStream");
	cmds.emplace_back(open_cmd);

	// Write  XXX this is wrong.
	Handle write_cmd =
		make_description("Write kernel and data to GPU",
		                 "WriteLink", "ItemNode");
	cmds.emplace_back(write_cmd);

	_global_desc = createLink(cmds, CHOICE_LINK);
}

// This is totally bogus because it is unused.
// This should be class static member
ValuePtr OpenclStream::describe(AtomSpace* as, bool silent)
{
	if (_description) return as->add_atom(_description);
	_description = as->add_atom(_global_desc);
	return _description;
}

// ==============================================================

void OpenclStream::update() const
{
	std::vector<double> result(_vec_dim);
	size_t vec_bytes = _vec_dim * sizeof(double);

	cl::Event event_handler;
	_queue.enqueueReadBuffer(_outvec, CL_TRUE, 0, vec_bytes, result.data(),
		nullptr, &event_handler);
	event_handler.wait();

	_value.resize(1);
	_value[0] = createNumberNode(result);
}

// ==============================================================
// Send kernel and data
ValuePtr OpenclStream::write_out(AtomSpace* as, bool silent,
                                 const Handle& cref)
{
	return do_write_out(as, silent, cref);
}

void OpenclStream::prt_value(const ValuePtr& kvec)
{
	if (0 == kvec->size())
		throw RuntimeException(TRACE_INFO,
			"Expecting a kernel name, got %s\n", kvec->to_string().c_str());

	// Unpack kernel name and kernel arguments
	std::string kern_name;
	Type tc = kvec->get_type();
	if (LIST_LINK == tc)
	{
		const HandleSeq& oset = HandleCast(kvec)->getOutgoingSet();
		if (not oset[0]->is_node())
			throw RuntimeException(TRACE_INFO,
				"Expecting Atom with kernel name, got %s\n",
				oset[0]->to_string().c_str());
		kern_name = oset[0]->get_name();

		// Find the shortest vector
		_vec_dim = UINT_MAX;
		for (size_t i=1; i<oset.size(); i++)
			if (oset[i]->size() < _vec_dim) _vec_dim = oset[i]->size();

		// XXX Assume floating point vectors FIXME
		_invec.clear();
		size_t vec_bytes = _vec_dim * sizeof(double);
		for (size_t i=1; i<oset.size(); i++)
		{
			NumberNodePtr np(NumberNodeCast(oset[i]));
			_invec.emplace_back(
				cl::Buffer(_context,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					vec_bytes,
					(void*) np->value().data()));
		}
	}
	else
		throw RuntimeException(TRACE_INFO,
			"Unknown data type: got %s\n", kvec->to_string().c_str());

	// XXX TODO this will throw exception if user mistyped the
	// kernel name. We should catch this and print a freindlier
	// error message.
	_kernel = cl::Kernel(_program, kern_name.c_str());

	// XXX Hardwired assumption about argument order.
	// FXIME... but how ???
	size_t vec_bytes = _vec_dim * sizeof(double);
	_outvec = cl::Buffer(_context, CL_MEM_READ_WRITE, vec_bytes);
	_kernel.setArg(0, _outvec);
	for (size_t i=1; i<kvec->size(); i++)
		_kernel.setArg(i, _invec[i-1]);

	// XXX This is the wrong thing to do in the long run.
	_kernel.setArg(kvec->size(), _vec_dim);

	// ------------------------------------------------------
	// Launch
	cl::Event event_handler;
	_queue.enqueueNDRangeKernel(_kernel,
		cl::NullRange,
		cl::NDRange(_vec_dim),
		cl::NullRange,
		nullptr, &event_handler);

	event_handler.wait();
}

// ==============================================================

// Adds factory when library is loaded.
DEFINE_VALUE_FACTORY(OPENCL_STREAM, createOpenclStream, std::string)
DEFINE_VALUE_FACTORY(OPENCL_STREAM, createOpenclStream, Handle)

// ====================================================================

void opencog_opencl_init(void)
{
   // Force shared lib ctors to run
};

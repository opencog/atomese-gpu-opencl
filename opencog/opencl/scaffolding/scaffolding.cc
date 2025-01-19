/**
 * OpenCL scaffolding.
 *
 * This provides minimalistic scaffolding to allow OpenCL experiments
 * to take place.
 *
 * Copyright (c) 2025 Linas Vepstas
 */

#include "scaffolding.h"

#include <CL/opencl.hpp>
#include <iostream>
#include <fstream>

/* ================================================================ */

/// Print rudimentary report of available OpenCL hardware.
void report_hardware(void)
{
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	printf("OpenCL Hardware report\n");
	printf("Should match what the `clinfo` tool reports.\n");
	printf("Found %ld platforms:\n", platforms.size());

	for (const auto& plat : platforms)
	{
		std::string pname = plat.getInfo<CL_PLATFORM_NAME>();
		std::string pvend = plat.getInfo<CL_PLATFORM_VENDOR>();
		std::string pvers = plat.getInfo<CL_PLATFORM_VERSION>();
		printf("Platform: %s\n", pname.c_str());
		printf("\tVendor: %s\n", pvend.c_str());
		printf("\tVersion: %s\n", pvers.c_str());

		std::vector<cl::Device> devices;
		plat.getDevices(CL_DEVICE_TYPE_ALL, &devices);
		// plat.getDevices(CL_DEVICE_TYPE_GPU, &devices);

		printf("\tThis platform has %ld GPU devices:\n", devices.size());

		for (const auto& dev: devices)
		{
			std::string dname = dev.getInfo<CL_DEVICE_NAME>();
			std::string dvers = dev.getInfo<CL_DEVICE_VERSION>();
			printf("\t\tDevice: %s\n", dname.c_str());
			printf("\t\tVersion: %s\n", dvers.c_str());

			unsigned int wdim = dev.getInfo<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS>();
			printf("\t\tWork item dimensions: %d\n", wdim);
			size_t maxsz = dev.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
			printf("\t\tMax work group size: %ld\n", maxsz);
			auto dimensions = dev.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
			printf("\t\tMax dimensions: %ld", dimensions[0]);
			for (unsigned int i=1; i<wdim; i++)
				printf(" x %ld", dimensions[i]);
			printf("\n");
			unsigned int svmcaps = dev.getInfo<CL_DEVICE_SVM_CAPABILITIES>();
			printf("\t\tSVM Caps bitflag: %x\n", svmcaps);

			printf("\n");
		}
		printf("\n");
	}
	printf("\n");
}

/* ================================================================ */

/// Return the first device that has platsubstr and devsubstr as
/// substrings in the platform and device name.
cl::Device find_device(const char* platsubstr, const char* devsubstr)
{
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);

	for (const auto& plat : platforms)
	{
		std::string pname = plat.getInfo<CL_PLATFORM_NAME>();
		if (pname.find(platsubstr) == std::string::npos)
			continue;

		std::vector<cl::Device> devices;
		// plat.getDevices(CL_DEVICE_TYPE_ALL, &devices);
		plat.getDevices(CL_DEVICE_TYPE_GPU, &devices);

		for (const cl::Device& dev: devices)
		{
			std::string dname = dev.getInfo<CL_DEVICE_NAME>();
			if (dname.find(devsubstr) == std::string::npos)
				continue;

			// Return first matching device.
			return dev;
		}
	}

	static const cl::Device nulldev;
	return nulldev;
}

/* ================================================================ */

/// Read source text file and build program.
/// Set return context and program as return results.
void build_kernel(cl::Context& context, const char* srcfile,
                  cl::Program& prog)
{
	// Copy in source code. Must be a better way!?
	fprintf(stderr, "Reading sourcefile %s\n", srcfile);
	std::ifstream srcfm(srcfile);
	// std::stringstream buffer;
	// buffer << srcfm.rdbuf();
	std::string src(std::istreambuf_iterator<char>(srcfm),
		(std::istreambuf_iterator<char>()));

	if (0 == src.size())
	{
		fprintf(stderr, "Error: Could not find file %s\n", srcfile);
		exit(1);
	}

	cl::Program::Sources sources;
	sources.push_back(src);

	cl::Program program(context, sources);

	// Compile
	fprintf(stderr, "Compiling sourcefile %s\n", srcfile);
	try
	{
		// Specifying flags causes exception.
		// program.build("-cl-std=CL1.2");
		program.build("");
	}
	catch (const cl::Error& e)
	{
		printf("Compile failed! %s\n", e.what());
#if 0
		printf("Log >>%s<<\n",
			program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(ocldev).c_str());
#endif
		exit(1);
	}

	prog = program;
}

/* ================================================================ */

/// Read SPV binary file and wrap it into a program.
/// Set return context and program as return results.
void load_kernel(cl::Context &context, const char* spvfile,
                 cl::Program& prog)
{
	// Copy in SPV file. Must be a better way!?
	fprintf(stderr, "Reading SPV file %s\n", spvfile);
	std::ifstream spvfm(spvfile);
	std::vector<char> spv(std::istreambuf_iterator<char>(spvfm),
		(std::istreambuf_iterator<char>()));

	if (0 == spv.size())
	{
		fprintf(stderr, "Error: Could not find file %s\n", spvfile);
		exit(1);
	}

	cl::Program program(context, spv);

	prog = program;
}

/* ======================= The End ================================ */

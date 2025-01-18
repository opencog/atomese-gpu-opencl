/**
 * OpenCL data flow demo.
 *
 * Simple demo of streaming float point data to/from GPU hardware.
 *
 * Copyright (c) 2025 Linas Vepstas
 */

#include "scaffolding.h"

// Wire user data into GPU
cl::Kernel setup_vec_mult(cl::Context context,
                          cl::Program program,
                          size_t vec_dim,
                          std::vector<double>& a,
                          std::vector<double>& b,
                          std::vector<double>& prod,
                          cl::Buffer& vec_results)
{
	size_t vec_bytes = vec_dim * sizeof(double);

	// Buffers holding data that will go to the GPU's
	// Buffer size is static.
	cl::Buffer veca(context,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, vec_bytes, a.data());

	cl::Buffer vecb(context,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, vec_bytes, b.data());

	cl::Buffer vecprod(context,
		CL_MEM_READ_WRITE, vec_bytes);

	// The program to run on the GPU, and the arguments it takes.
	cl::Kernel kernel(program, "vec_mult");
	kernel.setArg(0, vecprod);
	kernel.setArg(1, veca);
	kernel.setArg(2, vecb);
	kernel.setArg(3, vec_dim);

	vec_results = vecprod;
	return kernel;
}

// Launch
void queue_data(cl::Kernel kernel, cl::CommandQueue queue,
                 size_t vec_dim)
{
	cl::Event event_handler;
	queue.enqueueNDRangeKernel(kernel,
		cl::NullRange,
		cl::NDRange(vec_dim),
		cl::NullRange,
		nullptr, &event_handler);

	event_handler.wait();
	fprintf(stderr, "Done waiting on exec\n");
}

// Read
void get_results(cl::CommandQueue queue,
                size_t vec_dim,
                std::vector<double>& prod,
                cl::Buffer vecprod)
{
	cl::Event event_handler;
	size_t vec_bytes = vec_dim * sizeof(double);
	queue.enqueueReadBuffer(vecprod, CL_TRUE, 0, vec_bytes, prod.data(),
		nullptr, &event_handler);
	event_handler.wait();
	fprintf(stderr, "Done waiting on result read\n");
}

void run_flow (cl::Device ocldev,
               cl::Context context,
               cl::Program program)
{
	// Set up vectors
	size_t vec_dim = 64;
	std::vector<double> a(vec_dim);
	std::vector<double> b(vec_dim);
	std::vector<double> prod(vec_dim);

	// Product will be triangle numbers.
	for (size_t i=0; i<vec_dim; i++)
	{
		a[i] = (double) i;
		b[i] = 0.5 * (double) i+1;
		prod[i] = 0.0;
	}

	cl::Buffer results;
	cl::Kernel kern = setup_vec_mult(context, program,
	          vec_dim, a, b, prod, results);

	cl::CommandQueue queue(context, ocldev);

	queue_data(kern, queue, vec_dim);
	get_results(queue, vec_dim, prod, results);

	printf("The triangle numbers are:\n");
	for (size_t i=0; i<vec_dim; i++)
	{
		printf("%ld * %ld / 2 = %f\n", i, i+1, prod[i]);
	}
}

int main(int argc, char* argv[])
{
	cl::Device ocldev = find_device("", "AMD");
	std::string dname = ocldev.getInfo<CL_DEVICE_NAME>();
	printf("Will use: %s\n", dname.c_str());

	cl::Context ctxt;
	cl::Program prog;
	build_kernel(ocldev, "vec-mult.cl", ctxt, prog);
	run_flow(ocldev, ctxt, prog);
}

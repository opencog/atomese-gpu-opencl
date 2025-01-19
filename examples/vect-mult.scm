;
; vect-mult.scm
;
; Basic OpenGL GPU vector multiplication demo.
; Uses the Atomese "sensory" style API to open a channel to the
; OpenCL processing unit, and send data there.
;
; To run demo, copy `vec-mult.cl` in this directory to /tmp
; or alter the URL below.
;
(use-modules (opencog) (opencog exec))
(use-modules (opencog sensory) (opencog opencl))

; Optional; view debug messages
(use-modules (opencog logger))
(cog-logger-set-stdout! #t)

; URL specifying platform, device and program source.
; Modify as needed. Any substrings will do for platform and device,
; including empty strings. The first match found will be used.
; If in doubt, try 'opencl://:/tmp/vec-mult.c`
;
; Must be of the form 'opencl://platform/device/path/to/kernel.cl'
; Can also be clcpp or spv file.
; (define clurl "opencl://Clover:AMD Radeon/tmp/vec-mult.cl")
(define clurl "opencl://:/tmp/vec-mult.cl")

; Brute-force open. This checks the basic functions.
(cog-execute!
	(Open
		(Type 'OpenclStream)
		(SensoryNode clurl)))

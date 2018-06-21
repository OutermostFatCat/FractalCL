#ifdef MAC
#include "OpenCL.h/cl.h"
#else
#include "CL/cl.h"
#endif

#define FRACTAL_KERNEL_FILE "fractal_generation.cl"
#define NUM_FILES 1

#define W_WIDTH 900
#define W_HEIGHT 600

#define VERTEX_SHADER_FILE "fractal.vert"
#define FRAGMENT_SHADER_FILE "fractal.frag"

/* OpenCL */
#include <CL/cl_gl.h>

/* Standard Library */
#include <stdio.h>
#include <stdlib.h>

/* OpenGL */
#include <GL/glew.h>
#include <GL/glx.h>
#include <GLFW/glfw3.h>

/* CL */
cl_program cl_build_program(cl_context context, cl_device_id *devices, const size_t device_size);	
void gen_cl_texture(cl_context context ,cl_mem *cl_texture, GLuint gl_texture) ;
void cl_generate_fractal_texture(cl_command_queue queue, cl_kernel kernel, cl_mem *cl_texture,
	 const size_t *global_work_size, const size_t *local_work_size);

/* GL */
void gen_gl_texture(GLuint *texture, GLsizei width, GLsizei height);
void gen_gl_buffer(GLuint *vertex_buffer_object, GLuint *index_buffer_object);
void gen_gl_array_buffer(GLuint *vertex_array_object, GLuint *vertex_buffer_object, GLuint *index_buffer_object);
void gen_gl_sampler(GLuint gl_program, GLuint gl_texture);
void gl_initialize_program(GLuint *gl_program);
void gl_build_program(GLuint *gl_program, GLuint *shader_array);
GLuint gl_load_shader(GLenum shader_type, char *shader_file);
void display(GLuint gl_program, GLuint vertex_array_object);

/* GLFW */
void error_callback(int error, const char* description);
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

int main() {

	/* CL data */
	cl_platform_id *platforms;
	cl_context context;
	cl_device_id *devices;
	cl_program cl_program;
	cl_kernel kernel;
	cl_command_queue queue;

	cl_uint num_platforms;
	cl_int err;

	const char kernel_name[] = "fractal_generation";
	const size_t global_work_size[] = {W_WIDTH, W_HEIGHT};
 	const size_t local_work_size[] = {W_WIDTH, 1};

	/* OpenGL data */
	GLuint gl_program, gl_texture, vertex_array_object, vertex_buffer_object, index_buffer_object;
	GLFWwindow* window;
	GLsizei width = W_WIDTH;
	GLsizei height = W_HEIGHT;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
	    perror("Couldn't initiate gflw");
	    exit(1);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(width, height, "Fractal", NULL, NULL);
	if (!window) {   
		glfwTerminate();
   		perror("Couldn't create window");
   		exit(1);
	}

	glfwMakeContextCurrent(window);

	glfwSetKeyCallback(window, key_callback);

	GLenum error = glewInit();
	if (GLEW_OK != error) {
	  fprintf(stderr, "Error: %s\n", glewGetErrorString(error));
	  exit(1);
	}

	printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	err = clGetPlatformIDs(1, NULL, &num_platforms);
	platforms = malloc(sizeof(cl_platform_id));
	err |= clGetPlatformIDs(num_platforms, platforms, NULL);
	if (err < 0) {
		perror("Couldn't recieve platforms.");
		exit(1);
	}

	devices = malloc(sizeof(cl_device_id));
    err = clGetDeviceIDs(*platforms, CL_DEVICE_TYPE_GPU, 1, devices, NULL);
    if(err < 0) {
        perror("Couldn't find any devices");
        exit(1);
    }

	cl_context_properties properties[] = {
		CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
		CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
		CL_CONTEXT_PLATFORM, (cl_context_properties) platforms[0],
		0};
	context = clCreateContext(properties, 1, devices, NULL, NULL, &err);
	if (err < 0) {
		perror("Couldn't retrieve context.");
		exit(1);
	} 

	cl_program = cl_build_program(context, devices, sizeof(cl_device_id));

	kernel = clCreateKernel(cl_program, kernel_name, &err);
	if (err < 0) {
		perror("Couldn't create kernel");
		exit(1);
	}

	queue = clCreateCommandQueue(context, devices[0], CL_QUEUE_PROFILING_ENABLE, &err);
	if (err < 0) {
		perror("Couldn't create commandqueue");
		exit(1);
	}

	gl_initialize_program(&gl_program);
	gen_gl_buffer(&vertex_buffer_object, &index_buffer_object);
	gen_gl_array_buffer(&vertex_array_object, &vertex_buffer_object, &index_buffer_object);
	gen_gl_texture(&gl_texture, width, height);
	gen_gl_sampler(gl_program, gl_texture);

	cl_mem cl_texture;
	gen_cl_texture(context, &cl_texture, gl_texture);

	err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &cl_texture);
	if (err < 0) {
		perror("Couldn't set kernel argument");
		exit(1);
	}

	cl_generate_fractal_texture(queue, kernel, &cl_texture, global_work_size, local_work_size);

	while (!glfwWindowShouldClose(window))
	{	
		display(gl_program, vertex_array_object);
        glfwSwapBuffers(window);
        glfwPollEvents();
	}
		
	free(platforms);
	free(devices);

	clReleaseMemObject(cl_texture);
	clReleaseProgram(cl_program);
	clReleaseContext(context);
	clReleaseKernel(kernel);
	clReleaseCommandQueue(queue);

	/* OpenGL */
	glDeleteProgram(gl_program);
	glDeleteVertexArrays(1, &vertex_array_object);
	glDeleteBuffers(1, &index_buffer_object);
	glDeleteBuffers(1, &vertex_buffer_object);
	glDeleteTextures(1, &gl_texture);
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}


void display(GLuint gl_program, GLuint vertex_array_object) {

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(gl_program);
	glBindVertexArray(vertex_array_object);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	glUseProgram(0);
	glBindVertexArray(0);
}

cl_program cl_build_program(cl_context context, cl_device_id *devices, const size_t device_size) {

	FILE *program_handle;
	size_t file_size[NUM_FILES];
	size_t log_size;
	char *program_buffer[NUM_FILES];
	char *program_log;

	cl_int err;
	cl_uint i;

	const char *filename[] = {FRACTAL_KERNEL_FILE};
	const char program_options[] = "-cl-finite-math-only -cl-no-signed-zeros";

	for (i = 0; i < NUM_FILES; ++i) {
		
		program_handle = fopen(filename[i], "r");
		if (program_handle == NULL) {
			perror("Couldn't open the given file");
			exit(1);
		}

		/* Read files to buffer */
		fseek(program_handle, 0, SEEK_END);
		file_size[i] = ftell(program_handle);
		rewind(program_handle);
		program_buffer[i] = malloc(file_size[i]+1);
		program_buffer[i][file_size[i]] = '\0';
		fread(program_buffer[i], sizeof(char), file_size[i], program_handle);
		fclose(program_handle);
	}

	cl_program program = clCreateProgramWithSource(context, NUM_FILES, (const char**)program_buffer, file_size, &err);
	if (err < 0) {
		perror("Couldn't create Program.");
		exit(1);
	}

	err = clBuildProgram(program, device_size / sizeof(cl_device_id), devices, program_options, NULL, NULL);
	if (err < 0) {
		clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
		program_log = malloc(log_size+1);
		program_log[log_size] = '\0';
		clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, log_size+1, program_log, NULL);
   		fprintf(stderr, "(%i) Error building program: %s\n", err, program_log);
		free(program_log);
		exit(1);
	}

	for (i = 0; i < NUM_FILES; ++i){
		free(program_buffer[i]);
	}

	return program;
}

void gen_gl_texture(GLuint *texture, GLsizei width, GLsizei height) {

	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void gen_cl_texture(cl_context context ,cl_mem *cl_texture, GLuint gl_texture) {

	cl_int err;
	*cl_texture = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, gl_texture, &err);
	if (err < 0) {
		perror("Couldn't Create cl_texture from gl_texture");
		exit(1);
	}
}

void gen_gl_buffer(GLuint *vertex_buffer_object, GLuint *index_buffer_object) {

	const float vertex_data[] = {
	 
	   -1.0f,  1.0f, 0.0f,	// TOP LEFT
		1.0f, -1.0f, 0.0f,	// BOTTOM RIGHT
	   -1.0f, -1.0f, 0.0f,  // BOTTOM LEFT 
		1.0f,  1.0f, 0.0f,  // TOP RIGHT

		0.0f, 0.0f,			// Texture map coordinates
		1.0f, 1.0f,	
		0.0f, 1.0f,
		1.0f, 0.0f
	};

	const GLshort index_data[] =
	{
		0, 1, 2,
		0, 3, 1,
	};

	glGenBuffers(1, vertex_buffer_object);

	glBindBuffer(GL_ARRAY_BUFFER, *vertex_buffer_object);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, index_buffer_object);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *index_buffer_object);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_data), index_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void gen_gl_sampler(GLuint gl_program, GLuint gl_texture) {

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_texture);

	GLuint gl_texture_uniform = glGetUniformLocation(gl_program, "u_texture");
	glUseProgram(gl_program);
	glUniform1i(gl_texture_uniform, GL_TEXTURE0);
	glUseProgram(0);
}

unsigned int num_vertices = 4;
void gen_gl_array_buffer(GLuint *vertex_array_object, GLuint *vertex_buffer_object, GLuint *index_buffer_object) {

	glGenVertexArrays(1, vertex_array_object);
	glBindVertexArray(*vertex_array_object);

	size_t texture_offset = sizeof(float) * 3 * num_vertices;

	glBindBuffer(GL_ARRAY_BUFFER, *vertex_buffer_object);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*) texture_offset);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *index_buffer_object);

	glBindVertexArray(0);
}

void gl_initialize_program(GLuint *gl_program) {

	GLuint shader_array[2];

	shader_array[0] = gl_load_shader(GL_VERTEX_SHADER, VERTEX_SHADER_FILE);
	shader_array[1] = gl_load_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_FILE);

	gl_build_program(gl_program, shader_array);

	glDeleteShader(shader_array[0]);
	glDeleteShader(shader_array[1]);
}

GLuint gl_load_shader(GLenum shader_type, char *shader_file) {

	FILE *program_handle;
	size_t shader_size;
	char *shader_buffer[1];

	program_handle = fopen(shader_file, "r");
	if (program_handle == NULL) {
		perror("Couldn't open the given shader");
		exit(1);
	}

	fseek(program_handle, 0, SEEK_END);
	shader_size = ftell(program_handle);
	rewind(program_handle);
	shader_buffer[0] = malloc(shader_size + 1);
	shader_buffer[0][shader_size] = '\0';
	fread(*shader_buffer, sizeof(char), shader_size, program_handle);

	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, (const GLchar **) shader_buffer, (const GLint *) &shader_size);
	glCompileShader(shader);

	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE) {
		char *log;
		GLint log_size = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
		log = malloc(sizeof(log_size));
		glGetShaderInfoLog(shader, log_size, NULL, (GLchar *) log);

   		fprintf(stderr, "Error compiling shader: %s\n", log);

		glDeleteShader(shader);
		exit(1);
	}

	return shader;
}

void gl_build_program(GLuint *gl_program, GLuint *shader_array) {

	*gl_program = glCreateProgram();

	glAttachShader(*gl_program, shader_array[0]);
	glAttachShader(*gl_program, shader_array[1]);

	glLinkProgram(*gl_program);

	GLint isLinked = 0;
	glGetProgramiv(*gl_program, GL_LINK_STATUS, (int *)&isLinked);
	if (isLinked == GL_FALSE) {
		char *log;
		GLint length = 0;
		glGetProgramiv(*gl_program, GL_INFO_LOG_LENGTH, &length);
		log = malloc(sizeof(length));
		glGetProgramInfoLog(*gl_program, length, NULL, log);

		fprintf(stderr, "Error linking gl_program: %s\n", log);

		glDeleteProgram(*gl_program);
		glDeleteShader(shader_array[0]);
		glDeleteShader(shader_array[1]);
		exit(1);
	}

	glDetachShader(*gl_program, shader_array[0]);
	glDetachShader(*gl_program, shader_array[1]);
}

void cl_generate_fractal_texture(cl_command_queue queue, cl_kernel kernel, cl_mem *cl_texture,
	 const size_t *global_work_size, const size_t *local_work_size) {

	cl_event kernel_event;
	cl_ulong time_start, time_end;

	glFinish();
	clEnqueueAcquireGLObjects(queue, 1, cl_texture, 0, NULL, NULL);

	clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_work_size, local_work_size, 0, NULL, &kernel_event);
	clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &time_start, NULL);
	clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &time_end, NULL);
	time_end -= time_start;

	printf("kernel execumtion time (ns): %lu\n", time_end);

	clEnqueueReleaseGLObjects(queue, 1, cl_texture, 0, NULL, NULL);
	clFinish(queue);

	clReleaseEvent(kernel_event);
}

void error_callback(int error, const char* description) {
   
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

//x = 1000;
//if (x == 0) {
//	upload new coordinate
//	x = 1000
//}
//run kernel(vec_scale-xy)
//read new vec_scale-xy
//--x;
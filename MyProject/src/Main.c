#pragma region header files
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "array.h"
#include "display.h"
#include "vector.h"
#include "mesh.h"
#include "triangle.h"
#pragma endregion

int sorter_ascending(const void* first_arg, const void* second_arg)
{
	int first = *(int*)first_arg;
	int second = *(int*)second_arg;
	if (first > second)
	{
		return -1;
	}
	else if (first == second)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}
int sorter_descending(const void* first_arg, const void* second_arg)
{
	int first = *(int*)first_arg;
	int second = *(int*)second_arg;
	if (first < second)
	{
		return -1;
	}
	else if (first == second)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}
int partition(triangle_t arr[], int l, int r, float pivot, int (*compare)(const void*, const void*)) {
	while (l <= r) {
		while (compare(&arr[l].avg_depth, &pivot)==1) {
			l++;
		}
		while (compare(&arr[r].avg_depth, &pivot) == -1) {
			r--;
		}
		if (l <= r) {
			swap(arr[l], arr[r]);
			l++;
			r--;
		}
	}
	return l;
}
void quick_sort(triangle_t arr[], int l, int r, int (*compare)(const void*, const void*)) {
	if (l >= r) {
		return;
	}
	float pivot = arr[l + (r - l) / 2].avg_depth;
	int partition_index = partition(arr, l, r, pivot, compare);
	quick_sort(arr, 0, partition_index - 1, compare);
	quick_sort(arr, partition_index, r, compare);
}
// Function Prototypes
void free_resources(void);
// Number of triangles to be rendered = number of faces (since each triangle corresponds to one face)
triangle_t* triangles_to_render = NULL;

vec3_t camera_position = { .x = 0, .y = 0, .z = -10 };
//vec3_t cube_rotation = { .x = 0, .y = 0, .z = 0 };

float fov_factor = 1040;

bool is_running = false;
int previous_frame_time = 0;
// Rendering mode
enum renderMode { WireframePure, WireframeWithVertex, FilledTriangles, WireframeWithFilledTriangles};
bool backfaceCulling = false;
bool paintersAlgorithm = false;
enum  renderMode mode;
void setup(void) {
	// Allocate the required memory in bytes to hold the color buffer
	color_buffer = (uint32_t*)malloc(sizeof(uint32_t) * window_width * window_height);

	// Creating a SDL texture that is used to display the color buffer
	color_buffer_texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		window_width,
		window_height
	);
	load_cube_mesh_data();
	//load_obj_file_data("./assets/cube.obj");

	//rendering mode
	enum  renderMode mode = WireframePure;
	backfaceCulling = false;
	paintersAlgorithm = true;
}

void process_input(void) {
	SDL_Event event;
	SDL_PollEvent(&event);

	switch (event.type) {
	case SDL_QUIT:
		is_running = false;
		break;
	case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE)
			is_running = false;
		if (event.key.keysym.sym == SDLK_c)
			backfaceCulling = true;
		if (event.key.keysym.sym == SDLK_d)
			backfaceCulling = false;
		if (event.key.keysym.sym == SDLK_2)
			mode = WireframePure;
		if (event.key.keysym.sym == SDLK_1)
			mode = WireframeWithVertex;
		if (event.key.keysym.sym == SDLK_3)
			mode = FilledTriangles;
		if (event.key.keysym.sym == SDLK_4)
			mode = WireframeWithFilledTriangles;
		if (event.key.keysym.sym == SDLK_p)
			paintersAlgorithm = !paintersAlgorithm;

		break;

	}
}

////////////////////////////////////////////////////////////////////////////////
// Function that receives a 3D vector and returns a projected 2D point
////////////////////////////////////////////////////////////////////////////////
vec2_t project(vec3_t point) {
	vec2_t projected_point = {
		.x = (fov_factor * point.x) / point.z,
		.y = (fov_factor * point.y) / point.z
	};
	return projected_point;
}

void update(void) {
	triangles_to_render = NULL;
	// Returns an unsigned 32-bit value representing the number of milliseconds since the SDL library initialized.
	int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - previous_frame_time);

	// Only delay execution if we are running too fast
	if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
		SDL_Delay(time_to_wait);
	}
	previous_frame_time = SDL_GetTicks();

	mesh.rotation.x += 0.03;
	mesh.rotation.y += 0.03;
	mesh.rotation.z += 0.01;

	// Loop all triangle faces of our mesh
	// for a cube there are 6 sides = 6*2 triangular faces
	int numOfFaces = array_length(mesh.faces);
	for (int i = 0; i < numOfFaces; i++) {

		// get the face
		face_t mesh_face = mesh.faces[i];
		// for each face, fetch its three corresponding vertices (vec3)
		vec3_t face_vertices[3];
		face_vertices[0] = mesh.vertices[mesh_face.a - 1];
		face_vertices[1] = mesh.vertices[mesh_face.b - 1];
		face_vertices[2] = mesh.vertices[mesh_face.c - 1];

		vec3_t transformed_vertices[3];

		// Loop all three vertices of this current face and apply transformations
		for (int j = 0; j < 3; j++) {
			vec3_t transformed_vertex = face_vertices[j];

			// transform
			transformed_vertex = vec3_rotate_x(transformed_vertex, mesh.rotation.x);
			transformed_vertex = vec3_rotate_y(transformed_vertex, mesh.rotation.y);
			transformed_vertex = vec3_rotate_z(transformed_vertex, mesh.rotation.z);

			// Translate the vertex away from the camera
			transformed_vertex.z -= camera_position.z;
			transformed_vertices[j] = transformed_vertex;
		}
		// Back face culling
	
		if (backfaceCulling)
		{
			vec3_t BminusA = vec3_subtract(transformed_vertices[1], transformed_vertices[0]);
			vec3_t CminusA = vec3_subtract(transformed_vertices[2], transformed_vertices[0]);
			vec3_normalize(&CminusA);
			vec3_normalize(&BminusA);
			vec3_t normalToABC = vec3_crossProduct(BminusA, CminusA);
			vec3_normalize(&normalToABC);
			
			vec3_t cameraRay = vec3_subtract(camera_position, transformed_vertices[0]);
			float camRayDotFaceNormal = vec3_dotProduct(cameraRay, normalToABC);
			if (camRayDotFaceNormal < 0)
				continue;
		}
	
		// Loop all three vertices of this current face 
		// and apply perspective divide to the transformed vertices
		// 
		// an empty triangle
		triangle_t projected_triangle;
		for (int j = 0; j < 3; j++) {
			
			// Project the current vertex
			vec2_t projected_point = project(transformed_vertices[j]);

			// translate the projected points to the middle of the screen
			projected_point.x += (window_width / 2);
			projected_point.y += (window_height / 2);

			projected_triangle.points[j] = projected_point;
			// At this point, we know WHERE on the screen A vertex of a triangle should be painted
		}
		projected_triangle.color = mesh_face.color;
		projected_triangle.avg_depth = (transformed_vertices[0].z + 
			transformed_vertices[1].z + 
			transformed_vertices[2].z) / 3;
		// in an array of projected triangles, where each triangle has 3 points,
		// save those 3 points (ready to be painted) all at once, 
		// Save the projected triangle in the array of triangles to render
		//triangles_to_render[i] = projected_triangle;
		array_push(triangles_to_render, projected_triangle);
	}
	// Sort by Z buffer
	if (paintersAlgorithm)
	{
		quick_sort(triangles_to_render, 0, array_length(triangles_to_render) - 1, sorter_descending);
	}
		

}

void render(void) {
	draw_grid();
	int numOfTriangles = array_length(triangles_to_render);
	// Loop all projected triangles and render them
	for (int i = 0; i < numOfTriangles; i++) {
		triangle_t triangle = triangles_to_render[i];
		//draw_triangle_filled(triangle, RED, PURPLE, true);
		if (mode == WireframePure)
		{
			draw_triangle(triangle, RED, false);
		}
		else if (mode == WireframeWithVertex)
		{
			draw_triangle(triangle, RED, true);
		}
		else if (mode == WireframeWithFilledTriangles)
		{
			draw_triangle_filled(triangle, triangle.color, RED);
		}
		else if (mode == FilledTriangles)
		{
			draw_triangle_filled(triangle, PURPLE, PURPLE);
		}
		
	}

	//triangle_t triangle = { 
	//	.points[2].x = 300, .points[2].y = 100,
	//	.points[1].x = 650, .points[1].y = 400 ,
	//	.points[0].x = 500, .points[0].y = 700 
	//};
	//draw_triangle_filled(triangle, YELLOW, true);

	array_free(triangles_to_render);

	render_color_buffer();

	clear_color_buffer(0xFF000000);

	SDL_RenderPresent(renderer);
}

int main(void) {
	is_running = initialize_window();

	setup();

	while (is_running) {
		process_input();
		update();
		render();
	}

	destroy_window();
	free_resources();
	return 0;
}

void free_resources(void)
{
	array_free(mesh.faces);
	array_free(mesh.vertices);
	free(color_buffer);
}

#pragma once

#include <directxmath.h>
#include <d3d11_1.h>
#include <functional>

#include "MarchingCubeTable.h"
#include "Particle.h"


using namespace DirectX;
using namespace std;

struct Triangle
{
	int v1, v2, v3;
};

struct Vertex
{
	float x, y, z;
};

class MarchingCubes
{
public:
	MarchingCubes(const double _bound[], double _l, list<Particle>* _particles);
	~MarchingCubes();

	void MarchingCubesGrid();
	void MarchingCubesGenerate();


    void MarchingCubesMesh(vector<XMFLOAT3>& vertexs, vector<int>& tri_index);
private:
	const float isovalue = 100.0f;

	// return the closeset particle if there is a particle contain this vertex, otherwise NULL
	Particle* ParticleCheck(XMFLOAT3 v) const;

	// if one in one not, return the one in, otherwise, return NULL
	Particle* ParticleDifference(Particle* a, Particle* b) const;

	// return the intersect point one the edge
	XMFLOAT3* IntersectPoint(const XMFLOAT3& v, const XMFLOAT3& u, Particle* tmp) const;

    // position and size of the container of the particles
    XMFLOAT3 base, bound;

    // edge length of the cube
    double l;

    // number of edges on each coordinate
    int total_edge[3];

    // total number of vertex and edge
    int sum_v, sum_e;

    // list of particles
    list<Particle>* particles;

    /*	list of all edges for cubes
        point to the cut point if intersect with the mesh, otherwise NULL
        edge are numbered by related vetex towards z, y, x
     */
    vector<XMFLOAT3*> intersections;
    vector<int> offset_edge;
    int dx, dy, dz;

    // map edge to index of final mesh vertex
    vector<int> mapping;

    // list of all vertexs for cubes, point to the closest particle if it is inside, otherwise NULL
    vector<Particle*> inside;
    vector<int> offset;
};


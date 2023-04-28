#include "MarchingCubes.h"

MarchingCubes::MarchingCubes(const double _bound[], double _l, list<Particle>* _particles)
{
	
}


MarchingCubes::~MarchingCubes()
{
	
}

void MarchingCubes::MarchingCubesGrid()
{

}



void MarchingCubes::MarchingCubesGenerate()
{

}
XMFLOAT3* MarchingCubes::IntersectPoint(const XMFLOAT3& v, const XMFLOAT3& u, Particle* tmp) const
{
    return new XMFLOAT3((v.x + u.x) * 0.5f, (v.y + u.y) * 0.5f, (v.z + u.z) * 0.5f);
}

void MarchingCubes::MarchingCubesMesh(vector<XMFLOAT3>& vertexs, vector<int>& tri_index)
{
    vertexs.clear();
    tri_index.clear();

    // count inside
    int p = 0;
    for (int i = 0; i <= total_edge[0]; ++i) {
        for (int j = 0; j <= total_edge[1]; ++j) {
            for (int k = 0; k <= total_edge[2]; ++k, ++p) {
                XMFLOAT3 vertex(base[0] + i * l, base[1] + j * l, base[2] + k * l);
                inside[p] = ParticleCheck(vertex);
            }
        }
    }

    // cout intersection
    p = 0; Particle* tmp = NULL;
    for (int i = 0; i <= total_edge[0]; ++i) {
        for (int j = 0; j <= total_edge[1]; ++j) {
            for (int k = 0; k <= total_edge[2]; ++k, p += 3) {
                XMFLOAT3 v0(base[0] + i * l, base[1] + j * l, base[2] + k * l);
                XMFLOAT3 v1(base[0] + i * l + l, base[1] + j * l, base[2] + k * l);
                XMFLOAT3 v2(base[0] + i * l, base[1] + j * l + l, base[2] + k * l);
                XMFLOAT3 v3(base[0] + i * l, base[1] + j * l, base[2] + k * l + l);

                if (tmp = ParticleDifference(inside[p / 3], inside[p / 3 + dx])) {
                    intersections[p] = IntersectPoint(v0, v1, tmp);
                }
                if (tmp = ParticleDifference(inside[p / 3], inside[p / 3 + dy])) {
                    intersections[p + 1] = IntersectPoint(v0, v2, tmp);
                }
                if (tmp = ParticleDifference(inside[p / 3], inside[p / 3 + dz])) {
                    intersections[p + 2] = IntersectPoint(v0, v3, tmp);
                }
            }
        }
    }

    for (int i = 0; i < sum_e; ++i) {
        if (intersections[i]) {
            mapping[i] = vertexs.size();
            vertexs.push_back(*intersections[i]);
        }
    }

    // lookup
    p = 0;
    for (int i = 0; i < total_edge[0]; ++i) {
        for (int j = 0; j < total_edge[1]; ++j) {
            for (int k = 0; k < total_edge[2]; ++k, ++p) {

                int status = 0;
                for (int sta_i = 0, tw = 1; sta_i < 8; ++sta_i, tw = tw << 1) {
                    status |= inside[p + offset[sta_i]] ? tw : 0;
                }
                if (status == 0 || status == 255) continue;

                // Triangalation
                for (int tri_p = 0; tri_p < 16 && TRI_TABLE[status][tri_p] >= 0; ++tri_p) {
                    tri_index.push_back(mapping[offset_edge[TRI_TABLE[status][tri_p]] + p * 3]);
                }
            }
            p += dz;
        }
        p += dy;
    }
}

Particle* MarchingCubes::ParticleCheck(XMFLOAT3 v) const
{
	
}

Particle* MarchingCubes::ParticleDifference(Particle* a, Particle* b) const
{
	
}



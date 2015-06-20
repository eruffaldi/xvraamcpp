#pragma once
#include <Eigen/Core>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

namespace aamast
{
	class Material
	{
	public:
		std::string name;
		std::string materialclass;
		bool ismulti = false;

		virtual ~Material() {}
	};

	class Texture
	{
	public:
		bool specified = false;
		std::string filename;
		std::string mapping; // PYRAMIDAL
		float intensity = 1.0;
		Eigen::Vector3f uvwA = {0,0,0};
		Eigen::Vector2f uvO = {0,0};
		Eigen::Vector2f uvT = {0,0};
	};

	class MultiMaterial: public Material
	{
	public:
		MultiMaterial() { ismulti = true; }
		std::vector<std::shared_ptr<Material> > materials;
	};

	class StandardMaterial: public Material
	{
	public:
		Eigen::Vector3f ambient = {0,0,0};
		Eigen::Vector3f diffuse = {0,0,0};
		Eigen::Vector3f specular = {0,0,0};
		float shininess = 0;
		float transparency = 0;
		Texture tx;
		Texture ts;
	};

	class AAMGroup
	{
	public:
		//std::shared_ptr<Material> material;
		int materialsubidentifier = 0;
		int startface = 0; // in AAMObject
		int count = 0;     // in AAMObject
		int textureunits = 0; // computed
		int smoothinggroups = -1; // computed
	};



	class AAMObject
	{
	public:
		std::string name;
		int identifier;
		int parentidentifier;
		int materialidentifier;
		int totaltriangles;
		std::string userprop;

		std::shared_ptr<AAMObject> parent;
		//std::shared_ptr<Material> material;
		std::vector<Eigen::Vector2f> texturecoordinates;
		std::vector<std::shared_ptr<AAMGroup>> groups;
		std::vector<Eigen::Vector3f> vertices;

		std::vector<Eigen::Vector3i> ivertices;
		std::vector<int> smoothinggroup;
		std::vector<std::vector<Eigen::Vector3i>> itexturecoordinates;
	};

	
	class AAMFrame
	{
	public:
		// additional vertices per object
		int frameindex;	
	};

	// ANIMATIONS not supported
	class AAMMesh
	{
	public:
		std::vector<std::shared_ptr<AAMObject>> objects;
		std::vector<std::shared_ptr<AAMFrame>> frames;
		std::vector<std::shared_ptr<Material>> materials;

		int totalMaterials(std::vector<std::shared_ptr<Material>> * m = 0);
	};

	inline int AAMMesh::totalMaterials(std::vector<std::shared_ptr<Material>> * m)
	{
		int n = 0;
		if(!m)
			m = &materials;

		for(int i = 0; i <m->size(); i++)
		{
			if(!(*m)[i]->ismulti)
				n++;
			else
			{
				MultiMaterial * mm = (MultiMaterial*)((*m)[i]).get();
				n += totalMaterials(&(mm->materials));
			}
		}
		return n;
	}
}
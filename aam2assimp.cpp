/**
 * Apache License 2.0 - http://www.apache.org/licenses/LICENSE-2.0
 *
 * Emanuele Ruffaldi 2015
 */
#include "aamparse.hpp"
#include <iostream>
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <assimp/IOSystem.hpp>     // Post processing flags
#include <assimp/Exporter.hpp>     // Post processing flags
#include <map>
#include <list>
#include <tuple>

aiColor3D tocolor(const Eigen::Vector3f &c)
{
	return aiColor3D(c.x(),c.y(),c.z());
}

void convert(aiScene * scene, int & oi, std::vector<std::shared_ptr<aamast::Material>> & mm,std::map<aamast::Material*,int>&m2m, aamast::Material * pamat = 0)
{
	for (int i = 0; i < mm.size(); ++i)
	{
	//	std::cout << "converting material " << mm[i]->name << " class " << mm[i]->materialclass << std::endl;
		if(mm[i]->ismulti)
			convert(scene,oi,((aamast::MultiMaterial*)mm[i].get())->materials,m2m,mm[i].get());
		else
		{
			aamast::StandardMaterial * p = (aamast::StandardMaterial*)mm[i].get();
			std::cout << "convert standard material " << p->name << " " << " " << p->tx.filename << " " << p->ts.filename << std::endl;
			aiMaterial * m = new aiMaterial();
			m2m[p] = oi;
			scene->mMaterials[oi++] = m;
			std::string on = (pamat != 0 ? pamat->name + "_" +  p->name: p->name);
			//std::replace( on.begin(), on.end(), ' ', '_');
			aiString x(on);
			aiColor3D c;
			m->AddProperty(&p->shininess,1 ,AI_MATKEY_SHININESS);
			c = tocolor(p->diffuse);
			m->AddProperty(&c,1, AI_MATKEY_COLOR_DIFFUSE);
			c = tocolor(p->ambient);
			m->AddProperty(&c,1, AI_MATKEY_COLOR_AMBIENT);
			c = tocolor(p->specular);
			// lacks AI_MATKEY_COLOR_EMISSIVE
			// AI_MATKEY_REFRACTI
			m->AddProperty(&c,1, AI_MATKEY_COLOR_SPECULAR);
			m->AddProperty(&x,AI_MATKEY_NAME);
			float opa = 1.0-p->transparency;
			m->AddProperty(&opa,1,AI_MATKEY_OPACITY);

			if(!p->tx.filename.empty())
			{
				std::cout << "with texture " << p->tx.filename << std::endl;
				aamast::Texture & tex = p->tx;
				int clampMode = 0;
				int type = aiTextureType_DIFFUSE;
				int itexture = 0;
				aiString texname(tex.filename);
				m->AddProperty( &texname, AI_MATKEY_TEXTURE_DIFFUSE(0));
				//m->AddProperty<int>(&clampMode, 1, AI_MATKEY_MAPPINGMODE_U(type, 0));
			}
		}
	}
}

void normalize(aamast::AAMObject & alt, aamast::AAMObject * obj, aamast::AAMGroup * group)
{
	// count unique pairs of (ivertex,itexture1,..,itextureN) indices and reallocate them
	int nt = group->textureunits;
	if(nt > 1)
	{
		std::cerr << "unsupported more than 1 texture units\n";
		return;
	}
	std::map<std::tuple<int,int,int>,Eigen::Vector2i> idx2vtx;
	int ivtx = 0;
	alt.ivertices.resize(group->count); // number of faces


	for(int i = 0, iface = group->startface; i < group->count; i++,iface++)
	{
		// for every vertex
		for(int j = 0;j < 3; j++)
		{
			std::tuple<int,int,int> ii; // textur is 0 

			// build unique tuple of vertex (vertexindex,texture1,texture2)
			std::get<0>(ii) = obj->ivertices[iface](j); // original vertex for face and part
			std::get<1>(ii) = nt > 0 ? obj->itexturecoordinates[0][iface](j) : 0;
			std::get<2>(ii) = nt > 1 ? obj->itexturecoordinates[1][iface](j) : 0;
			auto it = idx2vtx.find(ii);
			if(it == idx2vtx.end())
			{
				// add new unique vertex, assigning the new index
				// add to the map and count
				alt.ivertices[i][j] = ivtx; 
				idx2vtx[ii] = Eigen::Vector2i(ivtx++,1);
			}
			else
			{
				// reuse the index of existent and increment usage
				alt.ivertices[i][j] = it->second.x();
				it->second.y()++;
			}
		}
	}
	std::cout << "\tgroup has " << ivtx << " unique vertices "<< std::endl;
	alt.texturecoordinates.resize(ivtx);
	alt.vertices.resize(ivtx);
	// no smoothing, not texture they are automatic
	for(auto & it : idx2vtx)
	{
		int oivtx = it.second.x(); // new index
		std::tuple<int,int,int> ii = it.first;
		alt.vertices[oivtx] = obj->vertices[std::get<0>(ii)];
		if(nt > 0) // write the texture coordintes (TODO if more than one, they have to stay separate)
			alt.texturecoordinates[oivtx] = obj->texturecoordinates[std::get<1>(ii)];
		//if(nt > 1)
		//	alt.texturecoordinates[oivtx] = obj->texturecoordinates[obj->itexturecoordinates[ii.y()]];
	}
}

void convert(aiScene * scene, aiNode * node, aamast::AAMMesh * mesh, aamast::AAMObject*obj, std::map<aamast::Material*,int> & m2m, int &imesh)
{
	// every group (or even smoothing group to a separate mesh)
	// in practice every group should be much like an obj with all vertices and co
	node->mNumMeshes = obj->groups.size();
	node->mMeshes = new unsigned int[node->mNumMeshes];
	for(int i = 0; i < obj->groups.size(); i++)
	{
		aiMesh * omesh = new aiMesh();
		node->mMeshes[i] = imesh;
		scene->mMeshes[imesh++] = omesh;

		// resolve material of group

		aamast::AAMObject alt;
		normalize(alt,obj,obj->groups[i].get());

		aamast::Material * mat = mesh->materials[obj->materialidentifier].get();
		if(mat->ismulti)
		{
			aamast::MultiMaterial * mm = (aamast::MultiMaterial *)mat;
			omesh->mMaterialIndex = m2m[mm->materials[obj->groups[i]->materialsubidentifier].get()];			
		}
		else
		{
			omesh->mMaterialIndex = m2m[mat];
		}
		std::cout << "\tgroup " << i << " mesh " << (imesh-1) << " attach material " << omesh->mMaterialIndex << std::endl;
		omesh->mNumFaces = alt.ivertices.size();
		omesh->mNumVertices = alt.vertices.size();

		// for all the texture units of the group
		int nt = obj->groups[i]->textureunits;
		for(int q = 0; q < nt; q++)
		{
			// 2 always 2 in AAM
			omesh->mNumUVComponents[q] = 2;
			// same as vertices in this model
			omesh->mTextureCoords[q] = new aiVector3D[omesh->mNumVertices]; // same
			for(int j = 0;  j < omesh->mNumVertices; j++)
				omesh->mTextureCoords[q][j] = aiVector3D(alt.texturecoordinates[j].x(),alt.texturecoordinates[j].y(),0);
		}


		omesh->mVertices = new aiVector3D[omesh->mNumVertices];
		omesh->mFaces = new aiFace[omesh->mNumFaces];
		for(int j = 0;  j <omesh->mNumVertices; j++)
			omesh->mVertices[j] = aiVector3D(alt.vertices[j].x(),alt.vertices[j].y(),alt.vertices[j].z());
		for(int j = 0;  j< omesh->mNumFaces ; j++)
		{
			aiFace & f = omesh->mFaces[j];
			f.mNumIndices = 3;
			f.mIndices = new unsigned int [3];
			// ?? better alloc?
			f.mIndices[0] = alt.ivertices[j].x();
			f.mIndices[1] = alt.ivertices[j].y();
			f.mIndices[2] = alt.ivertices[j].z();
		}
	}

}

int countmeshes(aamast::AAMMesh * mesh)
{	
	int n = 0;
	for (int i = 0; i < mesh->objects.size(); i++)
		n += mesh->objects[i]->groups.size(); 
	return n;
}

void convert(aiScene * scene, aamast::AAMMesh * mesh)
{
	// count number of materials
	std::map<aamast::Material*,int> m2m;
	int n = mesh->totalMaterials();
	int oi = 0;
	std::cout << "converting " << n << " materials" << std::endl;
	scene->mNumMaterials = n;
	scene->mMaterials = new aiMaterial*[n];
	convert(scene,oi,mesh->materials,m2m);

	scene->mNumMeshes = countmeshes(mesh);
	scene->mMeshes = new aiMesh*[scene->mNumMeshes];
 	
	// then proceed with objects
	std::map<int,aamast::AAMObject*> id2o;
	std::map<int,aiNode*> id2n;
	std::map<int,std::list<int> > children;

	aiNode * root = new aiNode();
	root->mName = "root";
	scene->mRootNode = root;
	id2n[-1] = root;

	std::cout << "converting " << mesh->objects.size() << " objects to nodes with "<< scene->mNumMeshes << " meshes" << std::endl;
	int imesh = 0;
	for(int i = 0; i < mesh->objects.size(); i++)
	{
		aamast::AAMObject * me = mesh->objects[i].get();
		id2o[me->identifier] = me;
		aamast::AAMObject * pa = me->parentidentifier == -1 ? 0 : id2o[me->parentidentifier];
		aiNode * node = new aiNode();
		node->mName = me->name;
		id2n[me->identifier] = node;
		children[me->parentidentifier].push_back(me->identifier);
		std::cout << "object node " << me->identifier << " " << me->parentidentifier << " " << me->name << " from mesh " << imesh << std::endl;
		convert(scene,node,mesh,me,m2m,imesh);
	}
	std::cout << "done " << imesh<< "meshes\n";

	// build hierarchy
	for(auto it = children.begin(); it != children.end(); it++)
	{
		aiNode * node = id2n[it->first];
		node->mNumChildren = it->second.size();
		node->mChildren = new aiNode*[node->mNumChildren];
		std::cout << "fix node children: " << it->first << " with " << node->mNumChildren << std::endl;
		auto l = it->second.begin();
		for(int i = 0; i < node->mNumChildren; ++i,l++)
		{
			node->mChildren[i] = id2n[*l];
			id2n[*l]->mParent = node;
		}
	}
}

void help()
{
	std::cout << "aam2assimp by Emanuele Ruffaldi 2015.\naam2assimp (--help|--formats|--collada|--obj)+ filename*" << std::endl;
	exit(0);
}

int main(int argc, char const *argv[])
{
	Assimp::Exporter e;
	bool todae = false;
	bool toobj = false;
	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i],"--help") == 0 || strcmp(argv[i],"-h") == 0)
			help();
		else if(strcmp(argv[i],"--collada") == 0)
			todae = true;
		else if(strcmp(argv[i],"--obj") == 0)
			toobj = true;
		else if(strcmp(argv[i],"--formats") == 0)
		{
			int n = e.GetExportFormatCount();
			for(int i = 0; i < n; i++)
			{
				auto p = e.GetExportFormatDescription(i);
				std::cout << " " << p->id << " " << p->description << " " << p->fileExtension << std::endl;
			}
		}
		else
		{
			std::cout << "parsing " << argv[i] << std::endl;
			std::string base = argv[i];
			AAMParser parser;
			aamast::AAMMesh * mesh = parser.parse(base);
			if(!mesh) 
			{
				std::cout <<" FAILED\n";
				continue;
			}
			std::string outpath;

			int k = base.rfind('.');
			if(k > 0)
				base = base.substr(0,k);

			aiScene scene;
			convert(&scene,mesh);

			if(todae)
			{
				outpath = base;
				outpath += ".dae";
				std::cout << "Generate: "<< outpath << std::endl;
				e.Export(&scene,"collada",outpath.c_str());
			}
			if(toobj)
			{
				outpath = base;
				outpath += ".obj";
				std::cout << "Generate: "<< outpath << std::endl;
				e.Export(&scene,"obj",outpath.c_str());
			}
			delete mesh;
		}
	}
	return 0;
}
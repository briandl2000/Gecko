#include "Rendering/Frontend/Scene/GLTFSceneLoader.h"

#include "Rendering/Frontend/Scene/Scene.h"
#include "Rendering/Frontend/ApplicationContext.h"

#include "Core/Asserts.h"
#include "Core/Logger.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <tiny_gltf.h>
#include <filesystem>

#include <stddef.h>
#include <stdint.h>

namespace Gecko
{

	void CalculateTangents(std::vector<Vertex3D>& vertices, const std::vector<u32>& indices)
	{
		std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.f));

		for (u32 i = 0; i < indices.size(); i += 3)
		{
			u32 i1 = indices[i + 0];
			u32 i2 = indices[i + 1];
			u32 i3 = indices[i + 2];

			Vertex3D& vertex1 = vertices[i2];
			Vertex3D& vertex2 = vertices[i1];
			Vertex3D& vertex3 = vertices[i3];

			glm::vec3 positionVertex1 = vertex1.position;
			glm::vec3 positionVertex2 = vertex2.position;
			glm::vec3 positionVertex3 = vertex3.position;

			glm::vec2 texCoordVertex1 = { vertex1.uv.x, -vertex1.uv.y };
			glm::vec2 texCoordVertex2 = { vertex2.uv.x, -vertex2.uv.y };
			glm::vec2 texCoordVertex3 = { vertex3.uv.x, -vertex3.uv.y };

			// Edges of the triangle : position delta
			glm::vec3 deltaPos1 = positionVertex2 - positionVertex1;
			glm::vec3 deltaPos2 = positionVertex3 - positionVertex1;

			// UV delta
			glm::vec2 deltaUV1 = texCoordVertex2 - texCoordVertex1;
			glm::vec2 deltaUV2 = texCoordVertex3 - texCoordVertex1;

			f32 r = 1.f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);

			glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
			glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

			vertex1.tangent += glm::vec4(tangent, 0.f);
			vertex2.tangent += glm::vec4(tangent, 0.f);
			vertex3.tangent += glm::vec4(tangent, 0.f);

			bitangents[i1] += bitangent;
			bitangents[i2] += bitangent;
			bitangents[i3] += bitangent;
		}

		for (u32 i = 0; i < vertices.size(); i++)
		{
			Vertex3D& vertex = vertices[i];
			glm::vec3 tangent = vertex.tangent;
			glm::vec3 bitangent = bitangents[i];
			glm::vec3 normal = vertex.normal;

			f32 h = (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f) ? -1.f : 1.f;

			tangent = glm::normalize(tangent);
			tangent = tangent - glm::dot(normal, tangent) * normal;
			tangent = glm::normalize(tangent);

			vertex.tangent = glm::vec4(tangent, h);
		}
	}

	void CalculateNormals(std::vector<Vertex3D>& vertices, const std::vector<u32>& indices)
	{
		for (u32 i = 0; i < indices.size(); i += 3)
		{
			Vertex3D& vertex1 = vertices[indices[i + 0]];
			Vertex3D& vertex2 = vertices[indices[i + 1]];
			Vertex3D& vertex3 = vertices[indices[i + 2]];

			glm::vec3 v1v2 = vertex2.position - vertex1.position;
			glm::vec3 v1v3 = vertex3.position - vertex1.position;

			glm::vec3 normal = glm::cross(v1v2, v1v3);

			vertex1.normal = glm::normalize(vertex1.normal + normal);
			vertex2.normal = glm::normalize(vertex2.normal + normal);
			vertex3.normal = glm::normalize(vertex3.normal + normal);
		}

		for (Vertex3D& vertex : vertices)
		{
			vertex.normal = glm::normalize(vertex.normal);
		}
	}
	
	[[nodiscard]] MeshHandle CreateMesh(ResourceManager* resourceManager, const tinygltf::Model& gltfModel, const tinygltf::Primitive& primitive)
	{
		std::vector<Vertex3D> vertices;
		std::vector<u32> indices;

		// indices
		{
			const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
			const tinygltf::BufferView& indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
			const tinygltf::Buffer& indexBuffer = gltfModel.buffers[indexBufferView.buffer];

			// Resize indices based on the amount of indices in the index accessor
			indices.resize(indexAccessor.count);
			for (u32 i = 0; i < indexAccessor.count; i++)
			{
				size_t index = indexBufferView.byteOffset + indexAccessor.byteOffset;

				if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT)
					indices[i] = static_cast<u32>(*reinterpret_cast<const short*>(&indexBuffer.data[index + i * sizeof(short)]));
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
					indices[i] = static_cast<u32>(*reinterpret_cast<const unsigned short*>(&indexBuffer.data[index + i * sizeof(unsigned short)]));
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_INT)
					indices[i] = static_cast<u32>(*reinterpret_cast<const int*>(&indexBuffer.data[index + i * sizeof(int)]));
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
					indices[i] = static_cast<u32>(*reinterpret_cast<const unsigned int*>(&indexBuffer.data[index + i * sizeof(unsigned int)]));
			}
		}

		// Positions
		{
			// Assume that every mesh has a POSITION attribute
			u32 accessorIndex = primitive.attributes.at("POSITION");
			const tinygltf::Accessor& accessor = gltfModel.accessors[accessorIndex];
			const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

			// Resize vertices based on the positions, because every vertex must have a position.
			vertices.resize(accessor.count);

			u32 stride = accessor.ByteStride(bufferView);
			u32 componentSizeInBytes = tinygltf::GetComponentSizeInBytes(accessor.componentType);
			u32 numComponentsInType = tinygltf::GetNumComponentsInType(accessor.type);

			ASSERT_MSG(componentSizeInBytes == 4, "POSITION needs to be in float format!");
			ASSERT_MSG(numComponentsInType == 3, "POSITION needs to have 3 components (x, y, z)!");

			u32 size = componentSizeInBytes * numComponentsInType;

			for (u32 i = 0; i < accessor.count; i++)
			{
				size_t index = bufferView.byteOffset + accessor.byteOffset + stride * i;
				Vertex3D& v = vertices[i];
				memcpy(&v.position, &buffer.data[index], size);
			}
		}

		// Uvs
		{
			// Assume that every mesh has a TEXCOORD attribute
			u32 accessorIndex = primitive.attributes.at("TEXCOORD_0");
			const tinygltf::Accessor& accessor = gltfModel.accessors[accessorIndex];
			const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

			u32 stride = accessor.ByteStride(bufferView);
			u32 componentSizeInBytes = tinygltf::GetComponentSizeInBytes(accessor.componentType);
			u32 numComponentsInType = tinygltf::GetNumComponentsInType(accessor.type);

			ASSERT_MSG(componentSizeInBytes == 4, "TEXCOORD_0 needs to be in float format!");
			ASSERT_MSG(numComponentsInType == 2, "TEXCOORD_0 needs to have 2 components (u, v)!");

			u32 size = componentSizeInBytes * numComponentsInType;

			for (u32 i = 0; i < accessor.count; i++)
			{
				size_t index = bufferView.byteOffset + accessor.byteOffset + stride * i;
				Vertex3D& v = vertices[i];
				memcpy(&v.uv, &buffer.data[index], size);
			}
		}

		// Normals
		{
			if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
			{
				u32 accessorIndex = primitive.attributes.at("NORMAL");
				const tinygltf::Accessor& accessor = gltfModel.accessors[accessorIndex];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

				u32 stride = accessor.ByteStride(bufferView);
				u32 componentSizeInBytes = tinygltf::GetComponentSizeInBytes(accessor.componentType);
				u32 numComponentsInType = tinygltf::GetNumComponentsInType(accessor.type);

				ASSERT_MSG(componentSizeInBytes == 4, "NORMAL needs to be in float format!");
				ASSERT_MSG(numComponentsInType == 3, "NORMAL needs to have 3 components (x, y, z)!");

				u32 size = componentSizeInBytes * numComponentsInType;

				for (u32 i = 0; i < accessor.count; i++)
				{
					size_t index = bufferView.byteOffset + accessor.byteOffset + stride * i;
					Vertex3D& v = vertices[i];
					memcpy(&v.normal, &buffer.data[index], size);
				}
			}
			else {
				CalculateNormals(vertices, indices);
			}
		}

		// Tangents
		{
			if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
			{
				u32 accessorIndex = primitive.attributes.at("TANGENT");
				const tinygltf::Accessor& accessor = gltfModel.accessors[accessorIndex];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

				u32 stride = accessor.ByteStride(bufferView);
				u32 componentSizeInBytes = tinygltf::GetComponentSizeInBytes(accessor.componentType);
				u32 numComponentsInType = tinygltf::GetNumComponentsInType(accessor.type);

				ASSERT_MSG(componentSizeInBytes == 4, "TANGENT needs to be in float format!");
				ASSERT_MSG(numComponentsInType == 4, "TANGENT needs to have 4 components (x, y, z, w)!");

				u32 size = componentSizeInBytes * numComponentsInType;

				for (u32 i = 0; i < accessor.count; i++)
				{
					size_t index = bufferView.byteOffset + accessor.byteOffset + stride * i;
					Vertex3D& v = vertices[i];
					memcpy(&v.tangent, &buffer.data[index], size);
				}
			}
			else {
				CalculateTangents(vertices, indices);
			}
		}

		// Create the Vertex3D and index buffers
		VertexBufferDesc vertexDesc;
		vertexDesc.Layout = Vertex3D::GetLayout();
		vertexDesc.VertexData = vertices.data();
		vertexDesc.NumVertices = static_cast<u32>(vertices.size());

		IndexBufferDesc indexDesc;
		indexDesc.IndexFormat = Format::R32_UINT;
		indexDesc.NumIndices = static_cast<u32>(indices.size());
		indexDesc.IndexData = indices.data();

		// TODO: check if a BLAS needs to be created.
		return resourceManager->CreateMesh(vertexDesc, indexDesc, false);
	}

	void LoadNodes(const tinygltf::Model& gltfModel, Scene* scene, SceneNode* sceneNode, u32 gltfNodeIndex, const std::vector<std::vector<SceneRenderObject>>& sceneRenderObjects)
	{
		const tinygltf::Node& gltfNode = gltfModel.nodes[gltfNodeIndex];
		
		// Get the transform for this node
		glm::vec3 translation = glm::vec3(0.f, 0.f, 0.f);
		glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
		glm::vec3 scale = glm::vec3(1.f, 1.f, 1.f);

		if (gltfNode.matrix.size() == 16)
		{
			glm::mat4 transform(0.f);

			for (u32 x = 0; x < 4; x++)
			{
				for (u32 y = 0; y < 4; y++)
				{
					transform[x][y] = static_cast<f32>(gltfNode.matrix[x + y * 4]);
				}
			}

			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(transform, scale, rotation, translation, skew, perspective);
		}
		else
		{
			if (gltfNode.translation.size() == 3)
			{
				translation.x = static_cast<f32>(gltfNode.translation[0]);
				translation.y = static_cast<f32>(gltfNode.translation[1]);
				translation.z = static_cast<f32>(gltfNode.translation[2]);
			}
			if (gltfNode.rotation.size() == 4)
			{
				rotation.x = static_cast<f32>(gltfNode.rotation[0]);
				rotation.y = static_cast<f32>(gltfNode.rotation[1]);
				rotation.z = static_cast<f32>(gltfNode.rotation[2]);
				rotation.w = static_cast<f32>(gltfNode.rotation[3]);
			}
			if (gltfNode.scale.size() == 3)
			{
				scale.x = static_cast<f32>(gltfNode.scale[0]);
				scale.y = static_cast<f32>(gltfNode.scale[1]);
				scale.z = static_cast<f32>(gltfNode.scale[2]);
			}
		}

		sceneNode->Transform.Position = translation;
		sceneNode->Transform.Rotation = glm::eulerAngles(rotation);
		sceneNode->Transform.Scale = scale;

		// Load the meshes into the Node
		if (gltfNode.mesh >= 0)
		{
			const std::vector<SceneRenderObject>& primitiveObjects = sceneRenderObjects[gltfNode.mesh];

			for (const SceneRenderObject& primitiveObject : primitiveObjects)
			{
				SceneRenderObject* sceneRenderObject = scene->CreateSceneRenderObject();
				sceneRenderObject->SetMeshHandle(primitiveObject.GetMeshHandle());
				sceneRenderObject->SetMaterialHandle(primitiveObject.GetMaterialHandle());
				Scope<SceneRenderObject> objPtr = CreateScopeFromRaw<SceneRenderObject>(sceneRenderObject);
				sceneNode->AppendSceneRenderObject(objPtr);
			}
		}

		// Load the nodes for this node
		for (u32 nodeIndex : gltfNode.children)
		{
			const tinygltf::Node& childGltfNode = gltfModel.nodes[nodeIndex];
			SceneNode* newNode = sceneNode->AddNode(childGltfNode.name);
			LoadNodes(gltfModel, scene, newNode, nodeIndex, sceneRenderObjects);
		}
	}

	[[nodiscard]] std::vector<TextureHandle> LoadTextures(ResourceManager* resourceManager, const tinygltf::Model& gltfModel)
	{
		std::vector<TextureHandle> textureHandles(gltfModel.textures.size());
		std::vector<TextureDesc> textureDescs(gltfModel.textures.size());

		for (u32 i = 0; i < gltfModel.textures.size(); i++)
		{
			const tinygltf::Texture& gltfTexture = gltfModel.textures[i];
			const tinygltf::Image& image = gltfModel.images[gltfTexture.source];

			TextureDesc& textureDesc = textureDescs[i];
			textureDesc.Width = image.width;
			textureDesc.Height = image.height;
			textureDesc.Type = TextureType::Tex2D;
			textureDesc.Format = Format::None;
			textureDesc.NumMips = CalculateNumberOfMips(textureDesc.Width, textureDesc.Height);
			textureDesc.NumArraySlices = 1;
		}

		for (u32 i = 0; i < gltfModel.materials.size(); i++)
		{
			const tinygltf::Material& gltfMaterial = gltfModel.materials[i];

			if (gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0)
				textureDescs[gltfMaterial.pbrMetallicRoughness.baseColorTexture.index].Format = Format::R8G8B8A8_SRGB;
			if (gltfMaterial.normalTexture.index >= 0)
				textureDescs[gltfMaterial.normalTexture.index].Format = Format::R8G8B8A8_UNORM;
			if (gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
				textureDescs[gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index].Format = Format::R8G8B8A8_UNORM;
			if (gltfMaterial.emissiveTexture.index >= 0)
				textureDescs[gltfMaterial.emissiveTexture.index].Format = Format::R8G8B8A8_SRGB;
			if (gltfMaterial.occlusionTexture.index >= 0)
				textureDescs[gltfMaterial.occlusionTexture.index].Format = Format::R8G8B8A8_UNORM;
		}

		for (u32 i = 0; i < gltfModel.textures.size(); i++)
		{
			const tinygltf::Texture& gltfTexture = gltfModel.textures[i];
			const tinygltf::Image& image = gltfModel.images[gltfTexture.source];

			if (textureDescs[i].Format == Format::None)
			{
				LOG_WARN("Texture %ui exists without being on a material!", i);
				textureHandles[i] = resourceManager->GetMissingTextureHandle();
			}
			else
			{
				textureHandles[i] = resourceManager->CreateTexture(
					textureDescs[i],
					const_cast<u8*>(image.image.data()),
					true
				);
			}
		}

		return textureHandles;
	}
	
	[[nodiscard]] std::vector<MaterialHandle> LoadMaterials(ResourceManager* resourceManager, const tinygltf::Model& gltfModel, const std::vector<TextureHandle> textureHandles)
	{
		std::vector<MaterialHandle> materialHandles(gltfModel.materials.size(), resourceManager->GetMissingMaterialHandle());

		// Load the materials
		for (u32 i = 0; i < gltfModel.materials.size(); i++)
		{
			materialHandles[i] = resourceManager->CreateMaterial();
			const tinygltf::Material& gltfMaterial = gltfModel.materials[i];

			Material& material = resourceManager->GetMaterial(materialHandles[i]);
			MaterialData* materialData = reinterpret_cast<MaterialData*>(material.MaterialConstantBuffer.Buffer);

			if (gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0) 
			{
				materialData->materialTextureFlags |= static_cast<u32>(MaterialTextureFlags::Albedo);
				material.AlbedoTextureHandle = textureHandles[gltfMaterial.pbrMetallicRoughness.baseColorTexture.index];
			}
			materialData->baseColorFactor[0] = static_cast<f32>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[0]);
			materialData->baseColorFactor[1] = static_cast<f32>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[1]);
			materialData->baseColorFactor[2] = static_cast<f32>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[2]);
			materialData->baseColorFactor[3] = static_cast<f32>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[3]);

			if (gltfMaterial.normalTexture.index >= 0) 
			{
				materialData->materialTextureFlags |= static_cast<u32>(MaterialTextureFlags::Normal);
				material.NormalTextureHandle = textureHandles[gltfMaterial.normalTexture.index];
			}
			materialData->normalScale = static_cast<f32>(gltfMaterial.normalTexture.scale);

			if (gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
			{
				materialData->materialTextureFlags |= static_cast<u32>(MaterialTextureFlags::MetalicRoughness);
				material.MetalicRoughnessTextureHandle = textureHandles[gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index];
			}
			materialData->matallicFactor = static_cast<f32>(gltfMaterial.pbrMetallicRoughness.metallicFactor);
			materialData->roughnessFactor = static_cast<f32>(gltfMaterial.pbrMetallicRoughness.roughnessFactor);

			if (gltfMaterial.emissiveTexture.index >= 0)
			{
				materialData->materialTextureFlags |= static_cast<u32>(MaterialTextureFlags::Emmisive);
				material.EmmisiveTextureHandle = textureHandles[gltfMaterial.emissiveTexture.index];
			}
			materialData->emissiveFactor[0] = static_cast<f32>(gltfMaterial.emissiveFactor[0]);
			materialData->emissiveFactor[1] = static_cast<f32>(gltfMaterial.emissiveFactor[1]);
			materialData->emissiveFactor[2] = static_cast<f32>(gltfMaterial.emissiveFactor[2]);

			if (gltfMaterial.occlusionTexture.index >= 0)
			{
				materialData->materialTextureFlags |= static_cast<u32>(MaterialTextureFlags::Occlusion);
				material.OcclusionTextureHandle = textureHandles[gltfMaterial.occlusionTexture.index];
			}
			materialData->occlusionStrength = static_cast<f32>(gltfMaterial.occlusionTexture.strength);
		}

		return materialHandles;
	}
	
	[[nodiscard]] std::vector<std::vector<SceneRenderObject>> LoadMeshes(ResourceManager* resourceManager, const tinygltf::Model& gltfModel, const std::vector<MaterialHandle>& materialHandles)
	{
		std::vector<std::vector<SceneRenderObject>> sceneRenderObjects(gltfModel.meshes.size());

		// load meshes
		for (u32 i = 0; i < gltfModel.meshes.size(); i++)
		{
			const tinygltf::Mesh& gltfMesh = gltfModel.meshes[i];

			std::vector<SceneRenderObject>& meshPrimitives = sceneRenderObjects[i];
			meshPrimitives.resize(gltfMesh.primitives.size());
			
			for (u32 j = 0; j < gltfMesh.primitives.size(); j++)
			{
				const tinygltf::Primitive& prim = gltfMesh.primitives[j];
				SceneRenderObject& sceneRenderObject = meshPrimitives[j];
				sceneRenderObject.SetMeshHandle(CreateMesh(resourceManager, gltfModel, prim));

				if (prim.material >= 0)
				{
					sceneRenderObject.SetMaterialHandle(materialHandles[prim.material]);
				}
				else
				{
					sceneRenderObject.SetMaterialHandle(resourceManager->GetMissingMaterialHandle());
				}
			}
		}

		return sceneRenderObjects;
	}

	u32 GLTFSceneLoader::LoadScene(const std::string& pathString, ApplicationContext& ctx)
	{

		const std::filesystem::path path = Gecko::Platform::GetLocalPath(pathString);

		// Get the gltf Mode
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		// Load the GLTF Model
		bool ret;
		if (path.extension() == ".gltf")
		{
			ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
		}
		else {
			ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
		}

		u32 sceneIdx = ctx.GetSceneManager()->CreateScene(path.filename().string());

		if (!warn.empty()) 
		{
			LOG_WARN(warn.c_str());
			return sceneIdx;
		}
		if (!err.empty()) 
		{
			LOG_ERROR(err.c_str());
			return sceneIdx;
		}
		if (!ret) 
		{
			LOG_WARN("Failed to parse glTF");
			return sceneIdx;
		}

		std::vector<TextureHandle> textureHandles = LoadTextures(ctx.GetResourceManager(), model);
		std::vector<MaterialHandle> materialHandles = LoadMaterials(ctx.GetResourceManager(), model, textureHandles);
		std::vector<std::vector<SceneRenderObject>> sceneRenderObjects = LoadMeshes(ctx.GetResourceManager(), model, materialHandles);

		// Loading nodes
		Scene* scene = ctx.GetSceneManager()->GetScene(sceneIdx);
		SceneNode* rootNode = scene->GetRootNode();

		for (const tinygltf::Scene& gltfScene : model.scenes)
		{
			for (u32 nodeIndex : gltfScene.nodes)
			{
				const tinygltf::Node& gltfNode = model.nodes[nodeIndex];
				SceneNode* newNode = rootNode->AddNode(gltfNode.name);
				LoadNodes(model, scene, newNode, nodeIndex, sceneRenderObjects);
			}
		}
		
		return sceneIdx;
	}

}
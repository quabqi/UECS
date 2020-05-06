#include <UECS/detail/Archetype.h>

#include <cstdlib>

using namespace std;
using namespace Ubpa;

Archetype::~Archetype() {
	for (size_t i = 0; i < num; i++) {
		size_t idxInChunk = i % chunkCapacity;
		byte* buffer = chunks[i / chunkCapacity]->Data();
		for (auto [id, so] : id2so) {
			auto [size, offset] = so;
			byte* address = buffer + offset + idxInChunk * size;
			RuntimeCmptTraits::Instance().Destruct(id, address);
		}
	}
	for (Chunk* chunk : chunks) {
#ifdef WIN32
		_aligned_free(chunk);
#else
		free(chunk);
#endif // WIN32
	}
}

size_t Archetype::RequestBuffer() {
	if (num == chunks.size() * chunkCapacity) {
#ifdef WIN32
		auto chunk = reinterpret_cast<Chunk*>(_aligned_malloc(sizeof(Chunk), std::alignment_of_v<Chunk>));
#else
		auto chunk = reinterpret_cast<Chunk*>(aligned_alloc(sizeof(Chunk), std::alignment_of_v<Chunk>));
#endif // WIN32
		chunks.push_back(chunk);
	}
	return num++;
}

tuple<void*, size_t> Archetype::At(size_t cmptID, size_t idx) const {
	assert(id2so.find(cmptID) != id2so.end());

	auto [size, offset] = id2so.find(cmptID)->second;
	size_t idxInChunk = idx % chunkCapacity;
	byte* buffer = chunks[idx / chunkCapacity]->Data();

	return { buffer + offset + idxInChunk * size,size };
}

size_t Archetype::Erase(size_t idx) {
	assert(idx < num);

	size_t dstIdxInChunk = idx % chunkCapacity;
	byte* dstBuffer = chunks[idx / chunkCapacity]->Data();

	size_t movedIdx;
	
	if (idx != num - 1) {
		movedIdx = num - 1;

		size_t srcIdxInChunk = movedIdx % chunkCapacity;
		byte* srcBuffer = chunks[movedIdx / chunkCapacity]->Data();

		for (auto [id, so] : id2so) {
			auto [size, offset] = so;
			byte* dst = dstBuffer + offset + dstIdxInChunk * size;
			byte* src = srcBuffer + offset + srcIdxInChunk * size;
			RuntimeCmptTraits::Instance().Destruct(id, dst);
			RuntimeCmptTraits::Instance().MoveConstruct(id, size, dst, src);
		}
	}
	else {
		movedIdx = static_cast<size_t>(-1);
		for (auto [id, so] : id2so) {
			auto [size, offset] = so;
			byte* dst = dstBuffer + offset + dstIdxInChunk * size;
			RuntimeCmptTraits::Instance().Destruct(id, dst);
		}
	}

	num--;

	if (chunks.size() * chunkCapacity - num >= chunkCapacity) {
		Chunk* chunk = chunks.back();
#ifdef WIN32
		_aligned_free(chunk);
#else
		free(chunk);
#endif // WIN32
		chunks.pop_back();
	}

	return movedIdx;
}

vector<CmptPtr> Archetype::Components(size_t idx) const {
	vector<CmptPtr> rst;

	for (const auto& [id, so] : id2so) {
		auto [cmpt, size] = At(id, idx);
		rst.emplace_back(id, cmpt);
	}

	return rst;
}

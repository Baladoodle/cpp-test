#include "Renderer.hpp"
#include <algorithm>

bool Renderer::render(
    Vec3 cameraPos,
    const std::vector<Chunk*>& selectedChunks,
    IndirectCommandDiagnostics* outDiagnostics
) {
    if (!geometryArena.isInitialized()) return false;

    std::vector<SectionGpuMetadata> drawMetadata;
    std::vector<DrawElementsIndirectCommand> drawCommands;
    drawMetadata.reserve(selectedChunks.size());
    drawCommands.reserve(selectedChunks.size());

    for (size_t i = 0; i < selectedChunks.size(); ++i) {
        Chunk* chunk = selectedChunks[i];
        if (!chunk) continue;

        SectionGpuMetadata meta;
        Vec3 minP(
            static_cast<float>(chunk->worldMin.x) - cameraPos.x,
            static_cast<float>(chunk->worldMin.y) - cameraPos.y,
            static_cast<float>(chunk->worldMin.z) - cameraPos.z
        );
        meta.chunkMinLod[0] = minP.x;
        meta.chunkMinLod[1] = minP.y;
        meta.chunkMinLod[2] = minP.z;
        meta.chunkMinLod[3] = static_cast<float>(chunk->lod);
        meta.sectionBounds[0] = static_cast<float>(chunk->worldSize);
        meta.sectionBounds[1] = static_cast<float>(1 << chunk->lod);

        DrawElementsIndirectCommand cmd;
        cmd.count = chunk->mesh.geometry.indexCount;
        cmd.instanceCount = 1;
        cmd.firstIndex = static_cast<uint32_t>(chunk->mesh.geometry.indexOffset);
        cmd.baseVertex = static_cast<int32_t>(chunk->mesh.geometry.baseVertex);
        cmd.baseInstance = static_cast<uint32_t>(i);

        drawMetadata.push_back(meta);
        drawCommands.push_back(cmd);
    }

    geometryArena.uploadDrawData(drawMetadata, drawCommands);
    geometryArena.drawIndirect(geometryArena.getLastDrawCommandCount());

    if (outDiagnostics) {
        *outDiagnostics = geometryArena.inspectIndirectCommands(geometryArena.getLastDrawCommandCount());
    }

    return true;
}

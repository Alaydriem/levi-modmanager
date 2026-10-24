#pragma once

#include <d3d11.h>

#include <array>

namespace amm_shim {

// saves the D3D11 pipeline state the UI draw changes and puts it back, so a draw on the game's own context leaves
// the game's state as it was. Render targets are not part of it
class D3D11StateBackup {
public:
    explicit D3D11StateBackup(ID3D11DeviceContext* ctx);
    ~D3D11StateBackup();

    D3D11StateBackup(D3D11StateBackup const&)            = delete;
    D3D11StateBackup& operator=(D3D11StateBackup const&) = delete;

private:
    static constexpr UINT MAX_CLASS_INSTANCES = 256;
    static constexpr UINT MAX_RECTS           = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

    ID3D11DeviceContext*                                    mCtx;
    UINT                                                    mScissorCount  = MAX_RECTS;
    UINT                                                    mViewportCount = MAX_RECTS;
    std::array<D3D11_RECT, MAX_RECTS>                       mScissors{};
    std::array<D3D11_VIEWPORT, MAX_RECTS>                   mViewports{};
    ID3D11RasterizerState*                                  mRaster = nullptr;
    ID3D11BlendState*                                       mBlend  = nullptr;
    std::array<FLOAT, 4>                                    mBlendFactor{};
    UINT                                                    mSampleMask = 0;
    ID3D11DepthStencilState*                                mDepth      = nullptr;
    UINT                                                    mStencilRef = 0;
    ID3D11ShaderResourceView*                               mPsView     = nullptr;
    ID3D11SamplerState*                                     mPsSampler  = nullptr;
    ID3D11PixelShader*                                      mPs         = nullptr;
    ID3D11VertexShader*                                     mVs         = nullptr;
    ID3D11GeometryShader*                                   mGs         = nullptr;
    ID3D11HullShader*                                       mHs         = nullptr;
    ID3D11DomainShader*                                     mDs         = nullptr;
    UINT                                                    mPsInstanceCount = MAX_CLASS_INSTANCES;
    UINT                                                    mVsInstanceCount = MAX_CLASS_INSTANCES;
    UINT                                                    mGsInstanceCount = MAX_CLASS_INSTANCES;
    std::array<ID3D11ClassInstance*, MAX_CLASS_INSTANCES>   mPsInstances{};
    std::array<ID3D11ClassInstance*, MAX_CLASS_INSTANCES>   mVsInstances{};
    std::array<ID3D11ClassInstance*, MAX_CLASS_INSTANCES>   mGsInstances{};
    ID3D11Buffer*                                           mVsConstants = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY                                mTopology    = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11Buffer*                                           mIndexBuffer = nullptr;
    DXGI_FORMAT                                             mIndexFormat = DXGI_FORMAT_UNKNOWN;
    UINT                                                    mIndexOffset = 0;
    ID3D11Buffer*                                           mVertexBuffer = nullptr;
    UINT                                                    mVertexStride = 0;
    UINT                                                    mVertexOffset = 0;
    ID3D11InputLayout*                                      mLayout       = nullptr;
};

} // namespace amm_shim

#include "render/D3D11StateBackup.h"

namespace amm_shim {

namespace {

template <class T>
void releaseAll(T* const* items, UINT count) {
    for (UINT i = 0; i < count; ++i) {
        if (items[i]) items[i]->Release();
    }
}

template <class T>
void releaseOne(T* item) {
    if (item) item->Release();
}

} // namespace

D3D11StateBackup::D3D11StateBackup(ID3D11DeviceContext* ctx) : mCtx(ctx) {
    mCtx->RSGetScissorRects(&mScissorCount, mScissors.data());
    mCtx->RSGetViewports(&mViewportCount, mViewports.data());
    mCtx->RSGetState(&mRaster);
    mCtx->OMGetBlendState(&mBlend, mBlendFactor.data(), &mSampleMask);
    mCtx->OMGetDepthStencilState(&mDepth, &mStencilRef);
    mCtx->PSGetShaderResources(0, 1, &mPsView);
    mCtx->PSGetSamplers(0, 1, &mPsSampler);
    mCtx->PSGetShader(&mPs, mPsInstances.data(), &mPsInstanceCount);
    mCtx->VSGetShader(&mVs, mVsInstances.data(), &mVsInstanceCount);
    mCtx->GSGetShader(&mGs, mGsInstances.data(), &mGsInstanceCount);
    mCtx->HSGetShader(&mHs, nullptr, nullptr);
    mCtx->DSGetShader(&mDs, nullptr, nullptr);
    mCtx->VSGetConstantBuffers(0, 1, &mVsConstants);
    mCtx->IAGetPrimitiveTopology(&mTopology);
    mCtx->IAGetIndexBuffer(&mIndexBuffer, &mIndexFormat, &mIndexOffset);
    mCtx->IAGetVertexBuffers(0, 1, &mVertexBuffer, &mVertexStride, &mVertexOffset);
    mCtx->IAGetInputLayout(&mLayout);
}

// every Get above added a reference; each Set takes its own, so the saved one is released after it
D3D11StateBackup::~D3D11StateBackup() {
    mCtx->RSSetScissorRects(mScissorCount, mScissors.data());
    mCtx->RSSetViewports(mViewportCount, mViewports.data());
    mCtx->RSSetState(mRaster);
    mCtx->OMSetBlendState(mBlend, mBlendFactor.data(), mSampleMask);
    mCtx->OMSetDepthStencilState(mDepth, mStencilRef);
    mCtx->PSSetShaderResources(0, 1, &mPsView);
    mCtx->PSSetSamplers(0, 1, &mPsSampler);
    mCtx->PSSetShader(mPs, mPsInstances.data(), mPsInstanceCount);
    mCtx->VSSetShader(mVs, mVsInstances.data(), mVsInstanceCount);
    mCtx->GSSetShader(mGs, mGsInstances.data(), mGsInstanceCount);
    mCtx->HSSetShader(mHs, nullptr, 0);
    mCtx->DSSetShader(mDs, nullptr, 0);
    mCtx->VSSetConstantBuffers(0, 1, &mVsConstants);
    mCtx->IASetPrimitiveTopology(mTopology);
    mCtx->IASetIndexBuffer(mIndexBuffer, mIndexFormat, mIndexOffset);
    mCtx->IASetVertexBuffers(0, 1, &mVertexBuffer, &mVertexStride, &mVertexOffset);
    mCtx->IASetInputLayout(mLayout);
    releaseOne(mRaster);
    releaseOne(mBlend);
    releaseOne(mDepth);
    releaseOne(mPsView);
    releaseOne(mPsSampler);
    releaseOne(mPs);
    releaseOne(mVs);
    releaseOne(mGs);
    releaseOne(mHs);
    releaseOne(mDs);
    releaseAll(mPsInstances.data(), mPsInstanceCount);
    releaseAll(mVsInstances.data(), mVsInstanceCount);
    releaseAll(mGsInstances.data(), mGsInstanceCount);
    releaseOne(mVsConstants);
    releaseOne(mIndexBuffer);
    releaseOne(mVertexBuffer);
    releaseOne(mLayout);
}

} // namespace amm_shim

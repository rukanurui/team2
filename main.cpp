#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include <cstdlib>      // srand,rand
#include <time.h>
using namespace DirectX;
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define DIRECTINPUT_VERSION 0x0800
#include<dinput.h>
#include<DirectXTex.h>
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#include <wrl.h>
using namespace Microsoft::WRL;

#include<d3dx12.h>

#include<xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#include<fstream>
LRESULT CALLBACK WindowProc(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


#ifdef _DEBUG
#   define MyOutputDebugString( str, ... ) \
      { \
        TCHAR c[256]; \
        _stprintf( c, str, __VA_ARGS__ ); \
        OutputDebugString( c ); \
      }
#else
#    define MyOutputDebugString( str, ... ) // 空実装
#endif

//スプライト用テクスチャ枚数
const int spriteSRVCount = 512;

struct PipeLineSet
{
    ComPtr<ID3D12PipelineState>pipelinestate;

    ComPtr<ID3D12RootSignature>rootsignature;
};

PipeLineSet object3dPipelineSet(ID3D12Device* dev);

struct Object3d
{
    ComPtr< ID3D12Resource>constBuff;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleCBV;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleCBV;

    XMFLOAT3 scale = { 2,0.1,2 };
    XMFLOAT3 rotation = { 0,0,0 };
    XMFLOAT3 position = { 0,0,200 };

    XMMATRIX matWorld;

    int Flag = 1;

    float Move = 0.0f;

    int Color = 0;

    int Rand_Flag = 0;
    int Rand_Cr = 0;

    float Size = 5.0f;

    Object3d* parent = nullptr;
};

//定数バッファデータ構造体
struct ConstBufferData8 {
    XMFLOAT4 color8;//色
    XMMATRIX mat8;//3D変換行列
};

struct VertexPosUv
{
    XMFLOAT3 pos;//x y z　座標
    XMFLOAT2 uv;//u v 座標
};


void InitializeObject3d(Object3d* object, int index, ComPtr<ID3D12Device> dev,
    ID3D12DescriptorHeap* descHeap);

void UpdateObject3d(Object3d* object, XMMATRIX& matView, XMMATRIX& matProjection, int Cr, int i, float rangeR, float rangeW, float rangeB);

void DrawObject3d(Object3d* object, ID3D12GraphicsCommandList* cmdList,
    ID3D12DescriptorHeap* descHeap, D3D12_VERTEX_BUFFER_VIEW& vbView,
    D3D12_INDEX_BUFFER_VIEW& ibView, D3D12_GPU_DESCRIPTOR_HANDLE
    gpuDescHandleSRV, UINT numIndices);

struct PipelineSet//パイプラインセット
{
    ComPtr<ID3D12PipelineState> pipelinestate;//パイプラインステートオブジェクト
    ComPtr<ID3D12RootSignature> rootsignature;//ルートシグネチャ
};

#pragma region 3dpipeline
PipelineSet Create3dpipe(ID3D12Device* dev)
{
    HRESULT result = S_OK;
    ComPtr<ID3DBlob> vsBlob;// ID3DBlob* vsBlob = nullptr; // 頂点シェーダオブジェクト
    ComPtr<ID3DBlob> psBlob;//ID3DBlob* psBlob = nullptr; // ピクセルシェーダオブジェクト
    ComPtr<ID3DBlob> errorBlob;//ID3DBlob* errorBlob = nullptr; // エラーオブジェクト

    // 頂点シェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"Resources/shaders/BasicVS.hlsl",  // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &vsBlob, &errorBlob);
    if (FAILED(result)) {
        // errorBlobからエラー内容をstring型にコピー
        std::string errstr;
        errstr.resize(errorBlob->GetBufferSize());

        std::copy_n((char*)errorBlob->GetBufferPointer(),
            errorBlob->GetBufferSize(),
            errstr.begin());
        errstr += "\n";
        // エラー内容を出力ウィンドウに表示
        OutputDebugStringA(errstr.c_str());
        exit(1);
    }

    // ピクセルシェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"Resources/shaders/BasicPS.hlsl",   // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &psBlob, &errorBlob);
    if (FAILED(result)) {
        // errorBlobからエラー内容をstring型にコピー
        std::string errstr;
        errstr.resize(errorBlob->GetBufferSize());

        std::copy_n((char*)errorBlob->GetBufferPointer(),
            errorBlob->GetBufferSize(),
            errstr.begin());
        errstr += "\n";
        // エラー内容を出力ウィンドウに表示
        OutputDebugStringA(errstr.c_str());
        exit(1);
    }


    // 頂点レイアウト
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }, // (1行で書いたほうが見やすい)

        {
            "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }, // (1行で書いたほうが見やすい)
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }, // (1行で書いたほうが見やすい)
    };
    CD3DX12_DESCRIPTOR_RANGE descRangeCBV, descRangeSRV;
    //descRangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);//b0 レジスタ
    descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);//t0 レジスタ


    CD3DX12_ROOT_PARAMETER rootparams[2];
    rootparams[0].InitAsConstantBufferView(0);//定数バッファビューとして初期化(b0　レジスタ)
    rootparams[1].InitAsDescriptorTable(1, &descRangeSRV, D3D12_SHADER_VISIBILITY_ALL);

    // グラフィックスパイプライン設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

    //頂点シェーダ　ピクセルシェーダ
    gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
    gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());

    //標準的な設定(背面をカリング ポリゴン内塗りつぶし 深度クリッピングを有効に)
    gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    //ブレンド
//gpipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;  // RBGA全てのチャンネルを描画
    D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
    blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    //共通設定(BLEND)
    blenddesc.BlendEnable = true;					//ブレンドを有効にする
    blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	//加算
    blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;		//ソースの値を100%使う
    blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;	//デストの値を0%使う

    //加算合成
    //blenddesc.BlendOp = D3D12_BLEND_OP_ADD;	 //加算
    //blenddesc.SrcBlend = D3D12_BLEND_ONE;	 //ソースの値を100%使う
    //blenddesc.DestBlend = D3D12_BLEND_ONE;	 //デストの値を100%使う

    //半透明合成
    blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
    blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

    gpipeline.InputLayout.pInputElementDescs = inputLayout;
    gpipeline.InputLayout.NumElements = _countof(inputLayout);
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gpipeline.NumRenderTargets = 1; // 描画対象は1つ
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
    gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

    //デプスステンシルステートの設定

    //標準的な設定(深度テストを行う 書き込み許可 小さければ合格)
    gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;//深度値フォーマット

    //デスクリプタテーブルの設定
    D3D12_DESCRIPTOR_RANGE descTblRange{};
    descTblRange.NumDescriptors = 1;
    descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descTblRange.BaseShaderRegister = 0;
    descTblRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //ルートパラメータ
    D3D12_ROOT_PARAMETER rootparam = {};
    rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootparam.DescriptorTable.pDescriptorRanges = &descTblRange;
    rootparam.DescriptorTable.NumDescriptorRanges = 1;
    rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;



    //ルートシグネチャ生成
    ComPtr<ID3D12RootSignature> rootsignature;



    //ルートシグネチャの設定
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

    rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    PipelineSet pipelineSet;

    ComPtr<ID3DBlob> rootSigBlob;

    result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
        &rootSigBlob, &errorBlob);

    result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&pipelineSet.rootsignature));

    gpipeline.pRootSignature = pipelineSet.rootsignature.Get();

    ComPtr<ID3D12PipelineState> pipelinestate;// ID3D12PipelineState* pipelinestate = nullptr;
    result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));

    return pipelineSet;
}
#pragma endregion


#pragma region 2dpipeline
PipelineSet CreateSprite2dpipe(ID3D12Device* dev)
{
    HRESULT result = S_OK;
    ComPtr<ID3DBlob> vsBlob;// ID3DBlob* vsBlob = nullptr; // 頂点シェーダオブジェクト
    ComPtr<ID3DBlob> psBlob;//ID3DBlob* psBlob = nullptr; // ピクセルシェーダオブジェクト
    ComPtr<ID3DBlob> errorBlob;//ID3DBlob* errorBlob = nullptr; // エラーオブジェクト

    // 頂点シェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"Resources/shaders/SpriteVS.hlsl",  // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &vsBlob, &errorBlob);
    if (FAILED(result)) {
        // errorBlobからエラー内容をstring型にコピー
        std::string errstr;
        errstr.resize(errorBlob->GetBufferSize());

        std::copy_n((char*)errorBlob->GetBufferPointer(),
            errorBlob->GetBufferSize(),
            errstr.begin());
        errstr += "\n";
        // エラー内容を出力ウィンドウに表示
        OutputDebugStringA(errstr.c_str());
        exit(1);
    }

    // ピクセルシェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"Resources/shaders/SpritePS.hlsl",   // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &psBlob, &errorBlob);
    if (FAILED(result)) {
        // errorBlobからエラー内容をstring型にコピー
        std::string errstr;
        errstr.resize(errorBlob->GetBufferSize());

        std::copy_n((char*)errorBlob->GetBufferPointer(),
            errorBlob->GetBufferSize(),
            errstr.begin());
        errstr += "\n";
        // エラー内容を出力ウィンドウに表示
        OutputDebugStringA(errstr.c_str());
        exit(1);
    }


    // 頂点レイアウト
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }, // (1行で書いたほうが見やすい)


        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }, // (1行で書いたほうが見やすい)
    };
    CD3DX12_DESCRIPTOR_RANGE  descRangeSRV;
    //descRangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);//b0 レジスタ
    descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);//t0 レジスタ


    CD3DX12_ROOT_PARAMETER rootparams[2];
    rootparams[0].InitAsConstantBufferView(0);//定数バッファビューとして初期化(b0　レジスタ)
    rootparams[1].InitAsDescriptorTable(1, &descRangeSRV, D3D12_SHADER_VISIBILITY_ALL);

    // グラフィックスパイプライン設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};
    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

    //頂点シェーダ　ピクセルシェーダ
    gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
    gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());


    //ブレンド
//gpipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;  // RBGA全てのチャンネルを描画
    D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
    blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    //共通設定(BLEND)
    blenddesc.BlendEnable = true;					//ブレンドを有効にする
    blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;	//加算
    blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;		//ソースの値を100%使う
    blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;	//デストの値を0%使う

    //加算合成
    //blenddesc.BlendOp = D3D12_BLEND_OP_ADD;	 //加算
    //blenddesc.SrcBlend = D3D12_BLEND_ONE;	 //ソースの値を100%使う
    //blenddesc.DestBlend = D3D12_BLEND_ONE;	 //デストの値を100%使う

    //半透明合成
    blenddesc.BlendOp = D3D12_BLEND_OP_ADD;
    blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

    gpipeline.InputLayout.pInputElementDescs = inputLayout;
    gpipeline.InputLayout.NumElements = _countof(inputLayout);
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gpipeline.NumRenderTargets = 1; // 描画対象は1つ
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
    gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

        //標準的な設定(ポリゴン内塗りつぶし 深度クリッピングを有効に)
    gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // 背面をカリングなし

    //デプスステンシルステートの設定

    //標準的な設定(書き込み許可)
    gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);//一旦標準値をセット
    gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;//常に上書き
    gpipeline.DepthStencilState.DepthEnable = false;//深度テストしない

    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;//深度値フォーマット

    //デスクリプタテーブルの設定
    D3D12_DESCRIPTOR_RANGE descTblRange{};
    descTblRange.NumDescriptors = 1;
    descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descTblRange.BaseShaderRegister = 0;
    descTblRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //ルートパラメータ
    D3D12_ROOT_PARAMETER rootparam = {};
    rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootparam.DescriptorTable.pDescriptorRanges = &descTblRange;
    rootparam.DescriptorTable.NumDescriptorRanges = 1;
    rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;



    //ルートシグネチャ生成
    ComPtr<ID3D12RootSignature> rootsignature;



    //ルートシグネチャの設定
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);

    rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    PipelineSet pipelineSet;

    ComPtr<ID3DBlob> rootSigBlob;

    result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
        &rootSigBlob, &errorBlob);

    result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&pipelineSet.rootsignature));

    gpipeline.pRootSignature = pipelineSet.rootsignature.Get();

    ComPtr<ID3D12PipelineState> pipelinestate;// ID3D12PipelineState* pipelinestate = nullptr;
    result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineSet.pipelinestate));

    return pipelineSet;
}
#pragma endregion 

struct Sprite
{
    //頂点バッファ
    ComPtr<ID3D12Resource> vertBuff;

    //頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView;

    //定数バッファ
    ComPtr<ID3D12Resource> constBuff;

    //XYZ軸周りの回転軸
    XMFLOAT3 rotation = { 0.0f,0.0f, 0.0f };

    //座標
    XMFLOAT3 position = { 0,0,0 };

    //ワールド行列
    XMMATRIX matWorld;

    //色
    XMFLOAT4 color = { 1,1,1,1 };

    //テクスチャ番号
    UINT texnumber = 0;

    //大きさ
    XMFLOAT2 size = { 100, 100 };

    //アンカーポイント
    XMFLOAT2 anchorpoint = { 0.5f,0.5f };

    //左右反転
    bool isFlagX = false;

    //上下反転
    bool isFlagY = false;

    //テクスチャ左上座標
    XMFLOAT2 texLeftTop = { 0,0 };

    //テクスチャ切り出しサイズ
    XMFLOAT2 texSize = { 100,100 };

    //非表示
    bool isInvisible = false;
};

struct SpriteCommon
{
    //パイプラインセット
    PipelineSet pipelineSet;

    //射影行列
    XMMATRIX matProjection{};

    //テクスチャ用デスクリプタヒープの生成
    ComPtr<ID3D12DescriptorHeap> descHeap;

    //テクスチャリソース(テクスチャバッファ)の配列
    ComPtr<ID3D12Resource> texBuff[spriteSRVCount];
};

void SpriteTransVertexBuffer(const Sprite& sprite, const SpriteCommon& spriteCommon)
{
    HRESULT result = S_FALSE;

    VertexPosUv vertices[] = {
        {{},{} },
        {{},{} },
        {{},{} },
        {{},{} },

    };

    //	 左下 左上 右下 右上
    enum { LB, LT, RB, RT };

    float left = (0.0f - sprite.anchorpoint.x) * sprite.size.x;

    float right = (1.0f - sprite.anchorpoint.x) * sprite.size.x;

    float top = (0.0f - sprite.anchorpoint.y) * sprite.size.y;

    float bottom = (1.0f - sprite.anchorpoint.y) * sprite.size.y;

    if (sprite.isFlagX == true)
    {
        left = -left;
        right = -right;
    }

    if (sprite.isFlagY == true)
    {
        top = -top;
        bottom = -bottom;
    }

    vertices[LB].pos = { left,	bottom, 0.0f };
    vertices[LT].pos = { left,	top,	0.0f };
    vertices[RB].pos = { right,	bottom, 0.0f };
    vertices[RT].pos = { right,	top,	0.0f };

    //UV
    //指定番号の画像が読み込み済みなら
    if (spriteCommon.texBuff[sprite.texnumber])
    {
        //テクスチャ情報取得
        D3D12_RESOURCE_DESC resDesc = spriteCommon.texBuff[sprite.texnumber]->GetDesc();

        float tex_left = sprite.texLeftTop.x / resDesc.Width;

        float tex_right = (sprite.texLeftTop.x + sprite.texSize.x) / resDesc.Width;

        float tex_top = sprite.texLeftTop.y / resDesc.Height;

        float tex_bottom = (sprite.texLeftTop.y + sprite.texSize.y) / resDesc.Height;

        vertices[LB].uv = { tex_left,	tex_bottom };
        vertices[LT].uv = { tex_left,	tex_top };
        vertices[RB].uv = { tex_right,	tex_bottom };
        vertices[RT].uv = { tex_right,	tex_top };

    }

    //頂点バッファへのデータ転送
    VertexPosUv* vertMap = nullptr;
    result = sprite.vertBuff->Map(0, nullptr, (void**)&vertMap);
    memcpy(vertMap, vertices, sizeof(vertices));
    sprite.vertBuff->Unmap(0, nullptr);
}

Sprite SpriteCreate(ID3D12Device* dev, int window_width, int window_height, UINT texnumber, const SpriteCommon& spriteCommon,
    XMFLOAT2 anchorpoint = { 0.5f,0.5f }, bool isFlagX = false, bool isFlagY = false)
{
    HRESULT result = S_FALSE;

    Sprite sprite{};

    //アンカーポイントをコピー
    sprite.anchorpoint = anchorpoint;

    //反転フラグをコピー
    sprite.isFlagX = isFlagX;

    sprite.isFlagY = isFlagY;

    VertexPosUv vertices[] = {
        {{0.0f	,100.0f	,0.0f},{0.0f,1.0f} },
        {{0.0f	,0.0f	,0.0f},{0.0f,0.0f} },
        {{100.0f,100.0f	,0.0f},{1.0f,1.0f} },
        {{100.0f,0.0f	,0.0f},{1.0f,0.0f} },


    };




    //頂点バッファの生成
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&sprite.vertBuff)
    );

    sprite.texnumber = texnumber;

    if (spriteCommon.texBuff[sprite.texnumber])
    {
        //テクスチャ情報の画像が読み込み済みなら
        D3D12_RESOURCE_DESC resDesc = spriteCommon.texBuff[sprite.texnumber]->GetDesc();

        //スプライトの大きさを画像の解像度に合わせる
        sprite.size = { (float)resDesc.Width,(float)resDesc.Height };
    }

    //頂点バッファデータ転送
    SpriteTransVertexBuffer(sprite, spriteCommon);




    //頂点バッファビューへのデータ転送
    VertexPosUv* vertMap = nullptr;
    result = sprite.vertBuff->Map(0, nullptr, (void**)&vertMap);
    memcpy(vertMap, vertices, sizeof(vertices));
    sprite.vertBuff->Unmap(0, nullptr);

    //頂点バッファビューの生成
    sprite.vbView.BufferLocation = sprite.vertBuff->GetGPUVirtualAddress();
    sprite.vbView.SizeInBytes = sizeof(vertices);
    sprite.vbView.StrideInBytes = sizeof(vertices[0]);

    //定数バッファの生成
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData8) + 0xff) & ~0xff),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&sprite.constBuff));

    //定数バッファにデータを転送
    ConstBufferData8* constMap = nullptr;
    result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
    constMap->color8 = XMFLOAT4(1, 1, 1, 1);//色指定(R G B A)

    //平行投影法
    constMap->mat8 = XMMatrixOrthographicOffCenterLH(
        0.0f, window_width, window_height, 0.0f, 0.0f, 1.0f);
    sprite.constBuff->Unmap(0, nullptr);

    return sprite;
}


SpriteCommon SpriteCommonCreate(ID3D12Device* dev, int window_width, int window_height)
{
    HRESULT result = S_FALSE;




    //新たなスプライト共通データを生成
    SpriteCommon spriteCommon{};

    //設定構造
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダーから見える
    descHeapDesc.NumDescriptors = spriteSRVCount;//テクスチャ枚数

    //デスクリプタヒープの生成
    result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&spriteCommon.descHeap));//生成

    //スプライト用パイプライン生成
    spriteCommon.pipelineSet = CreateSprite2dpipe(dev);

    //並行投影の射影行列変換
    spriteCommon.matProjection = XMMatrixOrthographicOffCenterLH(
        0.0f, (float)window_width, (float)window_height, 0.0f, 0.0f, 1.0f);


    return spriteCommon;
}

void SpriteCommonLoadTexture(SpriteCommon& spritecommon, UINT texnumber, const wchar_t* filename, ID3D12Device* dev)
{

    assert(texnumber <= spriteSRVCount - 1);

    HRESULT result;

    TexMetadata metadata{};
    ScratchImage scratchImg{};


    result = LoadFromWICFile(
        filename,
        WIC_FLAGS_NONE,
        &metadata, scratchImg);

    const Image* img = scratchImg.GetImage(0, 0, 0);

    const int texWidth = 256;//横方向ピクセル
    const int imageDataCount = texWidth * texWidth;//配列の要素数


    CD3DX12_HEAP_PROPERTIES texheapProp{};//テクスチャヒープ
    texheapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
    texheapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    texheapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;


    CD3DX12_RESOURCE_DESC texresDesc{};
    texresDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
    texresDesc.Format = metadata.format;
    texresDesc.Width = metadata.width;//幅
    texresDesc.Height = (UINT)metadata.height;//高さ
    texresDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
    texresDesc.MipLevels = (UINT16)metadata.mipLevels;
    texresDesc.SampleDesc.Count = 1;


    result = dev->CreateCommittedResource(//GPUリソースの生成
        &texheapProp,
        D3D12_HEAP_FLAG_NONE,
        &texresDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,//テクスチャ用指定
        nullptr,
        IID_PPV_ARGS(&spritecommon.texBuff[texnumber]));

    //リソース設定
    texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format,
        metadata.width,
        (UINT)metadata.height,
        (UINT16)metadata.arraySize,
        (UINT16)metadata.mipLevels);

    result = dev->CreateCommittedResource(//GPUリソースの生成
        &CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
        D3D12_HEAP_FLAG_NONE,
        &texresDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,//テクスチャ用指定
        nullptr,
        IID_PPV_ARGS(&spritecommon.texBuff[texnumber]));


    //テクスチャバッファにデータ転送
    result = spritecommon.texBuff[texnumber]->WriteToSubresource(
        0,
        nullptr,							//全領域にコピー
        img->pixels,						//元データアドレス
        (UINT)img->rowPitch,				//1ラインサイズ
        (UINT)img->slicePitch);			//全サイズ


    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(spritecommon.descHeap->GetCPUDescriptorHandleForHeapStart(),
            texnumber,
            dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV =
        CD3DX12_GPU_DESCRIPTOR_HANDLE(spritecommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
            texnumber,
            dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    //シェーダーリソースビュー設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};//設定構造体
    //srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;//RGBA
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
    srvDesc.Texture2D.MipLevels = 1;

    //ヒープの?番目にシェーダーリソースビューを生成
    dev->CreateShaderResourceView(spritecommon.texBuff[texnumber].Get(),//ビューと関連付けるバッファ
        &srvDesc,//テクスチャ設定情報
        CD3DX12_CPU_DESCRIPTOR_HANDLE(spritecommon.descHeap->GetCPUDescriptorHandleForHeapStart(), texnumber,
            dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

}

void SpriteUpdate(Sprite& sprite, const SpriteCommon& spriteCommon)
{
    //ワールド行列の更新
    sprite.matWorld = XMMatrixIdentity();

    //Z軸回転
    sprite.matWorld *= XMMatrixRotationZ(XMConvertToRadians(sprite.rotation.z));

    //平行移動
    sprite.matWorld *= XMMatrixTranslation(sprite.position.x, sprite.position.y, sprite.position.z);

    //定数バッファの転送
    ConstBufferData8* constMap = nullptr;
    HRESULT result = sprite.constBuff->Map(0, nullptr, (void**)&constMap);
    constMap->color8 = sprite.color;
    constMap->mat8 = sprite.matWorld * spriteCommon.matProjection;



    sprite.constBuff->Unmap(0, nullptr);
}

void SpriteCommonBeginDraw(ID3D12GraphicsCommandList* cmdList, const SpriteCommon& spriteCommon)
{
    //パイプラインステートの設定
    cmdList->SetPipelineState(spriteCommon.pipelineSet.pipelinestate.Get());

    //ルートシグネチャの設定
    cmdList->SetGraphicsRootSignature(spriteCommon.pipelineSet.rootsignature.Get());

    //プリミティブ形状を設定
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    //テクスチャ用デスクリプタヒープ
    ID3D12DescriptorHeap* ppHeaps[] = { spriteCommon.descHeap.Get() };
    cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}



void  SpriteDraw(const Sprite& sprite, ID3D12GraphicsCommandList* cmdList, SpriteCommon& spriteCommon, ID3D12Device* dev)
{

    if (sprite.isInvisible == true)
    {
        return;
    }
    //頂点バッファをセット
    cmdList->IASetVertexBuffers(0, 1, &sprite.vbView);

    //定数バッファをセット
    cmdList->SetGraphicsRootConstantBufferView(0, sprite.constBuff->GetGPUVirtualAddress());

    //
    cmdList->SetGraphicsRootDescriptorTable(1,
        CD3DX12_GPU_DESCRIPTOR_HANDLE(spriteCommon.descHeap->GetGPUDescriptorHandleForHeapStart(),
            sprite.texnumber,
            dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

    //ポリゴンの描画
    cmdList->DrawInstanced(4, 1, 0, 0);
}



class DebugText
{
public:
    static const int maxCharCount = 256;

    static const int fontWidth = 80;

    static const int fontHeight = 85;

    static const int fontLineCount = 16;

    Sprite sprites[maxCharCount];

    int spriteIndex = 0;

    void debugTextInit(ID3D12Device* dev, int window_width, int window_height, UINT texnumber, const SpriteCommon& spriteCommon);

    void Print(const SpriteCommon& spritecommon, const std::string& text, float x, float y, float scale = 1.0f);

    void DrawAll(ID3D12GraphicsCommandList* cmdList, SpriteCommon& spriteCommon, ID3D12Device* dev);
};

void DebugText::debugTextInit(ID3D12Device* dev, int window_width, int window_height, UINT texnumber, const SpriteCommon& spritecommon)
{
    for (int i = 0; i < _countof(sprites); i++)
    {
        sprites[i] = SpriteCreate(dev, window_width, window_height, texnumber, spritecommon, { 0,0 });
    }
}

void DebugText::Print(const SpriteCommon& spritecommon, const std::string& text, float x, float y, float scale)
{
    for (int i = 0; i < text.size(); i++)
    {
        if (spriteIndex >= maxCharCount)
        {
            break;
        }

        const unsigned char& charctor = text[i];

        int fontIndex = charctor - 32;

        if (charctor >= 0x7f)
        {
            fontIndex = 0;
        }

        int fontIndexX = fontIndex % fontLineCount;

        int fontIndexY = fontIndex / fontLineCount;

        sprites[spriteIndex].position = { x + fontWidth * scale * i,y,0 };
        sprites[spriteIndex].texLeftTop = { (float)fontIndexX * fontWidth,(float)fontIndexY * fontHeight };
        sprites[spriteIndex].texSize = { fontWidth,fontHeight };
        sprites[spriteIndex].size = { fontWidth * scale,fontHeight * scale };

        SpriteTransVertexBuffer(sprites[spriteIndex], spritecommon);

        SpriteUpdate(sprites[spriteIndex], spritecommon);

        spriteIndex++;


    }
}

void DebugText::DrawAll(ID3D12GraphicsCommandList* cmdList, SpriteCommon& spriteCommon, ID3D12Device* dev)
{
    for (int i = 0; i < spriteIndex; i++)
    {
        SpriteDraw(sprites[i], cmdList, spriteCommon, dev);
    }

    spriteIndex = 0;
}


struct ChunkHeader
{
    char id[4];
    int32_t size;
};

struct RiffHeader
{
    ChunkHeader chunk;
    char type[4];

};

struct FormatChunk
{
    ChunkHeader chunk;
    WAVEFORMATEX fmt;
};

struct SoundData
{
    WAVEFORMATEX wfex;

    BYTE* pBuffer;

    unsigned int bufferSize;


};

SoundData SoundLoadWave(const char* filename)
{
    HRESULT result;

    //File open
    std::ifstream file;

    file.open(filename, std::ios_base::binary);

    assert(file.is_open());

    //wavData Load
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));

    if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
    {
        assert(0);
    }

    if (strncmp(riff.type, "WAVE", 4) != 0)
    {
        assert(0);
    }

    FormatChunk format = {};

    file.read((char*)&format, sizeof(ChunkHeader));
    if (strncmp(format.chunk.id, "fmt ", 4) != 0)
    {
        assert(0);
    }

    assert(format.chunk.size <= sizeof(format.fmt));
    file.read((char*)&format.fmt, format.chunk.size);



    ChunkHeader data;
    file.read((char*)&data, sizeof(data));

    if (strncmp(data.id, "JUNK", 4) == 0)
    {
        file.seekg(data.size, std::ios_base::cur);

        file.read((char*)&data, sizeof(data));
    }

    if (strncmp(data.id, "LIST", 4) == 0)
    {
        file.seekg(data.size, std::ios_base::cur);

        file.read((char*)&data, sizeof(data));
    }

    if (strncmp(data.id, "data", 4) != 0)
    {
        assert(0);
    }

    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);

    file.close();
    //File close

    //return
    SoundData soundData = {};

    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
    soundData.bufferSize = data.size;

    

    return soundData;
}

void SoundUnload(SoundData* soundData)
{
    delete[] soundData->pBuffer;

    soundData->pBuffer = 0;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData)
{
    HRESULT result;

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;
   // buf.AudioBytes = size;

    result = pSourceVoice->SubmitSourceBuffer(&buf);
    result = pSourceVoice->Start();
}

void SoundPlayWaveLoop(IXAudio2* xAudio2, const SoundData& soundData)
{
    HRESULT result;

    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.LoopCount = XAUDIO2_LOOP_INFINITE; 
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // buf.AudioBytes = size;

    result = pSourceVoice->SubmitSourceBuffer(&buf);
    result = pSourceVoice->Start();
}



//# Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

    // コンソールへの文字出力
    OutputDebugStringA("Hello,DirectX!!\n");

    // ウィンドウサイズ
    const int window_width = 1280;  // 横幅
    const int window_height = 720;  // 縦幅

    WNDCLASSEX w{}; // ウィンドウクラスの設定
    w.cbSize = sizeof(WNDCLASSEX);
    w.lpfnWndProc = (WNDPROC)WindowProc; // ウィンドウプロシージャを設定
    w.lpszClassName = L"DirectXGame"; // ウィンドウクラス名
    w.hInstance = GetModuleHandle(nullptr); // ウィンドウハンドル
    w.hCursor = LoadCursor(NULL, IDC_ARROW); // カーソル指定

    // ウィンドウクラスをOSに登録
    RegisterClassEx(&w);
    // ウィンドウサイズ{ X座標 Y座標 横幅 縦幅 }
    RECT wrc = { 0, 0, window_width, window_height };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // 自動でサイズ補正

    // ウィンドウオブジェクトの生成
    HWND hwnd = CreateWindow(w.lpszClassName, // クラス名
        L"DirectXGame",         // タイトルバーの文字
        WS_OVERLAPPEDWINDOW,        // 標準的なウィンドウスタイル
        CW_USEDEFAULT,              // 表示X座標（OSに任せる）
        CW_USEDEFAULT,              // 表示Y座標（OSに任せる）
        wrc.right - wrc.left,       // ウィンドウ横幅
        wrc.bottom - wrc.top,   // ウィンドウ縦幅
        nullptr,                // 親ウィンドウハンドル
        nullptr,                // メニューハンドル

        w.hInstance,            // 呼び出しアプリケーションハンドル
        nullptr);               // オプション

    // ウィンドウ表示
    ShowWindow(hwnd, SW_SHOW);



    MSG msg{};  // メッセージ

// DirectX初期化処理　ここから
#ifdef _DEBUG
//デバッグレイヤーをオンに
    ID3D12Debug* debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    HRESULT result;
    ComPtr<ID3D12Device> dev = nullptr;
    ComPtr<IDXGIFactory6> dxgiFactory;
    ComPtr<IDXGISwapChain4>swapchain;
    ComPtr<ID3D12CommandAllocator>cmdAllocator;
    ComPtr<ID3D12GraphicsCommandList>cmdList;
    ComPtr<ID3D12CommandQueue>cmdQueue;
    ComPtr<ID3D12DescriptorHeap>rtvHeaps;

    ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice;


    // DXGIファクトリーの生成
    result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    // アダプターの列挙用
    std::vector<ComPtr<IDXGIAdapter1>> adapters;
    // ここに特定の名前を持つアダプターオブジェクトが入る
    ComPtr<IDXGIAdapter1>tmpAdapter;
    for (int i = 0;
        dxgiFactory->EnumAdapters1(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND;
        i++)
    {
        adapters.push_back(tmpAdapter); // 動的配列に追加する
    }

    for (int i = 0; i < adapters.size(); i++)
    {
        DXGI_ADAPTER_DESC1 adesc;
        adapters[i]->GetDesc1(&adesc);

        if (adesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        std::wstring strDesc = adesc.Description;
        if (strDesc.find(L"Intel") == std::wstring::npos)
        {
            tmpAdapter = adapters[i];
            break;
        }
    }

    D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL featureLevel;

    for (int i = 0; i < _countof(levels); i++)
    {
        // 採用したアダプターでデバイスを生成
        result = D3D12CreateDevice(tmpAdapter.Get(), levels[i], IID_PPV_ARGS(&dev));
        if (result == S_OK)
        {
            // デバイスを生成できた時点でループを抜ける
            featureLevel = levels[i];
            break;
        }
    }

    // コマンドアロケータを生成
    result = dev->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&cmdAllocator));

    // コマンドリストを生成
    result = dev->CreateCommandList(0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        cmdAllocator.Get(), nullptr,
        IID_PPV_ARGS(&cmdList));

    // 標準設定でコマンドキューを生成
    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc{};

    dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue));

    // 各種設定をしてスワップチェーンを生成
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
    swapchainDesc.Width = 1280;
    swapchainDesc.Height = 720;
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // 色情報の書式
    swapchainDesc.SampleDesc.Count = 1; // マルチサンプルしない
    swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER; // バックバッファ用
    swapchainDesc.BufferCount = 2;  // バッファ数を２つに設定
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後は破棄
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ComPtr<IDXGISwapChain1> swapchain1;

    result = dxgiFactory->CreateSwapChainForHwnd(
        cmdQueue.Get(),
        hwnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        &swapchain1);

    swapchain1.As(&swapchain);

    // 各種設定をしてデスクリプタヒープを生成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
    heapDesc.NumDescriptors = 2;    // 裏表の２つ
    dev->CreateDescriptorHeap(&heapDesc,
        IID_PPV_ARGS(&rtvHeaps));

    // 裏表の２つ分について
    std::vector<ComPtr<ID3D12Resource>> backBuffers(2);
    for (int i = 0; i < 2; i++)
    {
        // スワップチェーンからバッファを取得
        result = swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
        // デスクリプタヒープのハンドルを取得
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        // 裏か表かでアドレスがずれる
        handle.ptr += i * dev->GetDescriptorHandleIncrementSize(heapDesc.Type);
        // レンダーターゲットビューの生成
        dev->CreateRenderTargetView(
            backBuffers[i].Get(),
            nullptr,
            handle);
    }

    // フェンスの生成
    ComPtr<ID3D12Fence>fence;
    UINT64 fenceVal = 0;

    result = dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    IDirectInput8* dinput = nullptr;
    result = DirectInput8Create(
        w.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&dinput, nullptr);
    IDirectInputDevice8* devkeyboard = nullptr;
    result = dinput->CreateDevice(GUID_SysKeyboard, &devkeyboard, NULL);
    result = devkeyboard->SetDataFormat(&c_dfDIKeyboard);
    result = devkeyboard->SetCooperativeLevel(
        hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);


   

    result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);

    //マスターボイスを作成
    result = xAudio2->CreateMasteringVoice(&masterVoice);

    // DirectX初期化処理　ここまで

    //変数宣言
    float x = 0;
    float y = 0;
    int Color = 0.1;
    float Green = 0;
    float Red = 1;

    float r = 1;
    float g = 1;
    float b = 1;

    //ゲーム用変数2469行へ
   
    //描画初期化処理


        //定数バッファデータ構造体
    struct ConstBufferData8 {
        XMFLOAT4 color;//色
        XMMATRIX mat;//3D変換行列
    };

    //スプライト初期化
    SpriteCommon spriteCommon;
    spriteCommon = SpriteCommonCreate(dev.Get(), window_width, window_height);

    SpriteCommonLoadTexture(spriteCommon, 0, L"Resources/title.png", dev.Get());
    SpriteCommonLoadTexture(spriteCommon, 1, L"Resources/gameover.png", dev.Get());

    Sprite sprite;

    sprite = SpriteCreate(dev.Get(), window_width, window_height, 0, spriteCommon);

    sprite.texnumber = 0;

    Sprite sprite1;

    sprite1 = SpriteCreate(dev.Get(), window_width, window_height, 1, spriteCommon);

    sprite1.texnumber = 1;


    DebugText debugtext;

    const int debugTextTexNumber = 2;

    SpriteCommonLoadTexture(spriteCommon, debugTextTexNumber, L"Resources/ASC_white1280.png", dev.Get());

    debugtext.debugTextInit(dev.Get(), window_width, window_height, debugTextTexNumber, spriteCommon);

    DebugText debugtext2;

    const int debugTextTexNumber2 = 3;

    SpriteCommonLoadTexture(spriteCommon, debugTextTexNumber2, L"Resources/ASC_white1280gr.png", dev.Get());

    debugtext2.debugTextInit(dev.Get(), window_width, window_height, debugTextTexNumber2, spriteCommon);

    



    ComPtr<ID3D12Resource> depthBuffer;

    CD3DX12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,
        window_width,
        window_height,
        1, 0,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthResDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
        IID_PPV_ARGS(&depthBuffer)
    );





    //深度ビューでスクリプタヒープ
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ComPtr< ID3D12DescriptorHeap>dsvHeap;
    result = dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

    //深度ビュー作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dev->CreateDepthStencilView(
        depthBuffer.Get(),
        &dsvDesc,
        dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );


    ComPtr<ID3D12Resource> constBuff;
    result = dev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer((sizeof(ConstBufferData8) + 0xff) & ~0xff),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&constBuff));



    ConstBufferData8* constMap = nullptr;
    result = constBuff->Map(0, nullptr, (void**)&constMap);//マッピング
    constMap->color = XMFLOAT4(1, 0, 0, 0.5f);//半透明の赤
    constBuff->Unmap(0, nullptr);//マッピング削除

    constMap->mat = XMMatrixIdentity();

    //constMap->mat.r[0].m128_f32[0] = 2.0f / 1280;
    //constMap->mat.r[1].m128_f32[1] = -2.0f / 720;

    constMap->mat = XMMatrixOrthographicOffCenterLH(
        200.0f, 1080.0f,
        620.0f, 100.0f,
        256.0f, 0.0f
    );

    constMap->mat = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(60.0f),//上下角度
        (float)1280 / 720,//アスペクト比
        0.1f, 1000.0f//前端、奥端
    );


    XMMATRIX matProjection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(60.0f),
        (float)window_width / window_height,
        0.1f, 1000.0f
    );
    constMap->mat = matProjection;

    //ビュー変換行列
    XMMATRIX matView;
    XMFLOAT3 eye(0, 30, -80);
    XMFLOAT3 target(0, 0, 0);
    XMFLOAT3 up(0, 1, 0);
    matView = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

    constMap->mat = matView * matProjection;



    //ワールド変換
    XMMATRIX matWorld;
    matWorld = XMMatrixIdentity();


    XMMATRIX matScale;//スケーリング行列
    matScale = XMMatrixScaling(1.0f, 0.5f, 1.0f);
    matWorld *= matScale;

    XMMATRIX matRot;//回転行列
    matRot = XMMatrixIdentity();
    matRot *= XMMatrixRotationZ(XMConvertToRadians(45.0f));
    matRot *= XMMatrixRotationX(XMConvertToRadians(45.0f));
    matRot *= XMMatrixRotationY(XMConvertToRadians(45.0f));
    matWorld *= matRot;



    XMMATRIX matTrans;//平行移動行列
    matTrans = XMMatrixTranslation(70.0f, 0, 0);
    matWorld *= matTrans;

    XMFLOAT3 scale;
    XMFLOAT3 rotation;
    XMFLOAT3 position;

    scale = { 1.0f,1.0f,1.0f };
    rotation = { 0.0f,0.0f,0.0f };
    position = { 0.0f,-50.0f,0.0f };

    // XMFLOAT3  position = { 50.0f,0.0f,0.0f };

    constMap->mat = matWorld * matView * matProjection;
    /*
    */
    //定数バッファ用
    ComPtr< ID3D12DescriptorHeap>basicDescHeap;
    //設定構造体
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//
    descHeapDesc.NumDescriptors = 2;//シェーダ
    //デスクリフヒープの生成
    result = dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

    //でスクリプタヒープの戦闘バンドルを取得
    D3D12_CPU_DESCRIPTOR_HANDLE basicHeapHandle =
        basicDescHeap->GetCPUDescriptorHandleForHeapStart();

    //CD3DX12_CPU_DESCRIPTOR_HANDLE basicHeapHandle =
      //  CD3DX12_CPU_DESCRIPTOR_HANDLE(basicDescHeap->GetCPUDescriptorHandleForHeapStart(),

        //    dev->GetDescriptorHandleIncrementSize(/*D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV*/heapDesc.Type));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = (UINT)constBuff->GetDesc().Width;
    dev->CreateConstantBufferView(
        &cbvDesc, basicHeapHandle);

    //デスクリスタテーブルの設定
    //
    /*
    D3D12_DESCRIPTOR_RANGE descTblRangeCBV{};
    descTblRangeCBV.NumDescriptors = 1;//定数一つ
    descTblRangeCBV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//種別は定数
    descTblRangeCBV.BaseShaderRegister = 0;//スロットから
    descTblRangeCBV.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//標準
     //デスクリスタテーブルの設定
    D3D12_DESCRIPTOR_RANGE descTblRangeSRV{};
    descTblRangeSRV.NumDescriptors = 1;//定数一つ
    descTblRangeSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//種別は定数
    descTblRangeSRV.BaseShaderRegister = 0;//スロットから
    descTblRangeSRV.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//標準
    //
    D3D12_ROOT_PARAMETER rootparams[2] = {};
    //定数用
    rootparams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//種類
    rootparams[0].DescriptorTable.pDescriptorRanges = &descTblRangeCBV;//でスクリプタレンジ
    rootparams[0].DescriptorTable.NumDescriptorRanges = 1;//デスクリプタレンジ数
    rootparams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//全てのシェーダから見える
    //テクスチャ用
    rootparams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//種類
    rootparams[1].DescriptorTable.pDescriptorRanges = &descTblRangeSRV;//でスクリプタレンジ
    rootparams[1].DescriptorTable.NumDescriptorRanges = 1;//デスクリプタレンジ数
    rootparams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//全てのシェーダから見える
    */
    CD3DX12_DESCRIPTOR_RANGE descRangeCBV, descRangeSRV;
    descRangeCBV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    descRangeSRV.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootparams[2];
    rootparams[0].InitAsDescriptorTable(1, &descRangeCBV);
    rootparams[1].InitAsDescriptorTable(1, &descRangeSRV);

    //テクスチャバッファ
    /*const int texWidth = 256;//横方向ピクセル
    const int imageDataCount = texWidth * texWidth;//配列の要素数
    //画像イメージデータ配列
    XMFLOAT4* imageData = new XMFLOAT4[imageDataCount];//必ず後で開放する
    //全ピクセルの色を初期化
    for (int i = 0; i < imageDataCount; i++)
    {
        imageData[i].x = 1.0f;//R
        imageData[i].y = 0.0f;//G
        imageData[i].z = 0.0f;//B
        imageData[i].w = 1.0f;//A
    }*/

    TexMetadata   metadata{};
    ScratchImage scratchImg{};

    result = LoadFromWICFile(
       // L"Resources/unnamed.jpg",
        L"Resources/White.png",
        WIC_FLAGS_NONE,
        &metadata, scratchImg
    );

    const Image* img = scratchImg.GetImage(0, 0, 0);


    //
    /*
    D3D12_HEAP_PROPERTIES texHeapProp{};//テクスチャ―ヒープ設定
    texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
    texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    D3D12_RESOURCE_DESC texresDesc{};//リソース設定
    texresDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);//2Dテクスチャ要
    texresDesc.Format = metadata.format;//RGBAフォーマット
    texresDesc.Width = metadata.width;//幅
    texresDesc.Height = (UINT)metadata.height;//高さ
    texresDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
    texresDesc.MipLevels = (UINT16)metadata.mipLevels;
    texresDesc.SampleDesc.Count = 1;
    */
    //
    CD3DX12_RESOURCE_DESC texresDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format,
        metadata.width,
        (UINT)metadata.height,
        (UINT16)metadata.arraySize,
        (UINT16)metadata.mipLevels
    );

    //テクスチャバッファの生成
    /*
    ComPtr<ID3D12Resource>texbuff;
    result = dev->CreateCommittedResource(//GPUリソース設定
        &texHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &texresDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,//テクスチャ用指定
        nullptr,
        IID_PPV_ARGS(&texbuff)
    );
    */
    ComPtr<ID3D12Resource>texbuff = nullptr;
    result = dev->CreateCommittedResource(//GPUリソース設定
        &CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
        D3D12_HEAP_FLAG_NONE,
        &texresDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&texbuff));




    //でスクリプタ1個分アドレスをまとめる
    basicHeapHandle.ptr +=
        dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //シェーダリソースビュー設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};//設定構造体
    srvDesc.Format = metadata.format;//DXGI_FORMAT_R32G32B32A32_FLOAT;//RGBA
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
    srvDesc.Texture2D.MipLevels = 1;

    //ヒープの2番目にシェーダーリソースビュー作成
    dev->CreateShaderResourceView(texbuff.Get(),//ビューと関連づけるバッファ
        &srvDesc,//テクスチャ設定情報
        basicHeapHandle
    );

    //テクスチャバッファにデータ転送
    result = texbuff->WriteToSubresource(
        0,
        nullptr,//全領域へコピー
        img->pixels,//元データアドレス
        (UINT)img->rowPitch,//1ラインサイズ
        (UINT)img->slicePitch//全サイズ
    );

    //元データ開放


    struct Vertex
    {
        XMFLOAT3 pos;//xyz座標
        XMFLOAT3 normal;//法線ベクトル
        XMFLOAT2 uv;//uv座標
    };
    const float topHeight = 10.0f;//天井の高さ

    //頂点データ
    Vertex vertices[] = {

        {{-5.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},//左下
        {{-5.0f,+5.0f,-5.0f},{}, {0.0f,0.0f}},//左上
        {{+5.0f,-5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        {{+5.0f,+5.0f,-5.0f},{}, {1.0f,0.0f}},//右上
        //後

        {{-5.0f,+5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{-5.0f,-5.0f,5.0f},{}, {0.0f,1.0f}},//左下
        {{+5.0f,+5.0f,5.0f},{}, {1.0f,0.0f}},//右上
        {{+5.0f,-5.0f,5.0f},{}, {1.0f,1.0f}},//右下
        //左
        {{-5.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},//左下
        {{-5.0f,-5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{-5.0f, 5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        {{-5.0f,+5.0f,5.0f},{}, {1.0f,0.0f}},//右上
        //右

        {{5.0f,-5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{5.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},//左下
        {{5.0f,+5.0f,5.0f},{}, {1.0f,0.0f}},//右上
        {{5.0f, 5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        //下
        {{-5.0f,5.0f,5.0f},{}, {0.0f,1.0f}},//左下
        {{5.0f,5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{-5.0f,5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        {{5.0f,+5.0f,-5.0f},{}, {1.0f,0.0f}},//右上
        //上
        {{-5.0f,-5.0f,5.0f},{}, {0.0f,1.0f}},//左下
        {{-5.0f,-5.0f,-5.0f},{}, {0.0f,0.0f}},//左上
        {{5.0f,-5.0f,5.0f},{}, {1.0f,1.0f}},//右下
        {{5.0f,-5.0f,-5.0f},{}, {1.0f,0.0f}},//右上


    };


    constMap->mat.r[3].m128_f32[0] = -1.0f;
    constMap->mat.r[3].m128_f32[1] = 1.0f;
    //インデックスデータ
    unsigned short indices[] = {

        0,1,2,//三角形一つ目
        2,1,3,//二つ目
        //
        4,5,6,
        6,5,7,
        //
        8,9,10,
        10,9,11,
        //
        12,13,14,
        14,13,15,
        //
        16,17,18,
        18,17,19,
        //
        20,21,22,
        22,21,23


    };



    for (int i = 0; i < 36 / 3; i++)
    {
        unsigned short indices0 = indices[i * 3 + 0];
        unsigned short indices1 = indices[i * 3 + 1];
        unsigned short indices2 = indices[i * 3 + 2];

        XMVECTOR p0 = XMLoadFloat3(&vertices[indices0].pos);
        XMVECTOR p1 = XMLoadFloat3(&vertices[indices1].pos);
        XMVECTOR p2 = XMLoadFloat3(&vertices[indices2].pos);

        XMVECTOR v1 = XMVectorSubtract(p1, p0);
        XMVECTOR v2 = XMVectorSubtract(p2, p0);

        XMVECTOR normal = XMVector3Cross(v1, v2);

        normal = XMVector3Normalize(normal);

        XMStoreFloat3(&vertices[indices0].normal, normal);
        XMStoreFloat3(&vertices[indices1].normal, normal);
        XMStoreFloat3(&vertices[indices2].normal, normal);
    }

    // 頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
    UINT sizeVB = static_cast<UINT>(sizeof(Vertex) * _countof(vertices));

    // 頂点バッファの設定
    /*
    D3D12_HEAP_PROPERTIES heapprop{};   // ヒープ設定
    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD; // GPUへの転送用
    D3D12_RESOURCE_DESC resdesc{};  // リソース設定
    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resdesc.Width = sizeVB; // 頂点データ全体のサイズ
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.SampleDesc.Count = 1;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // 頂点バッファの生成
    ComPtr< ID3D12Resource>vertBuff;
    result = dev->CreateCommittedResource(
        &heapprop, // ヒープ設定
        D3D12_HEAP_FLAG_NONE,
        &resdesc, // リソース設定
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertBuff));
    *///
    ComPtr< ID3D12Resource>vertBuff;
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeVB),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertBuff)
    );

    // GPU上のバッファに対応した仮想メモリを取得
    Vertex* vertMap = nullptr;
    result = vertBuff->Map(0, nullptr, (void**)&vertMap);

    // 全頂点に対して
    for (int i = 0; i < _countof(vertices); i++)
    {
        vertMap[i] = vertices[i];   // 座標をコピー
    }

    // マップを解除
    vertBuff->Unmap(0, nullptr);

    // 頂点バッファビューの作成
    D3D12_VERTEX_BUFFER_VIEW vbView{};

    vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
    vbView.SizeInBytes = sizeVB;
    vbView.StrideInBytes = sizeof(Vertex);

    ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
    ComPtr< ID3DBlob> psBlob; // ピクセルシェーダオブジェクト
    ComPtr< ID3DBlob> errorBlob; // エラーオブジェクト


    // 頂点シェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"Resources/shaders/BasicVS.hlsl",  // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &vsBlob, &errorBlob);
    // ピクセルシェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"Resources/shaders/BasicPS.hlsl",   // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &psBlob, &errorBlob);

    if (FAILED(result)) {
        // errorBlobからエラー内容をstring型にコピー
        std::string errstr;
        errstr.resize(errorBlob->GetBufferSize());

        std::copy_n((char*)errorBlob->GetBufferPointer(),
            errorBlob->GetBufferSize(),
            errstr.begin());
        errstr += "\n";
        // エラー内容を出力ウィンドウに表示
        OutputDebugStringA(errstr.c_str());
        exit(1);
    }

    // 頂点レイアウト
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }, // (1行で書いたほうが見やすい)
        {
            "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
        },
        {
            "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
        },
    };

    // グラフィックスパイプライン設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};

    //デプスステンシルステートの設定
    /*
    gpipeline.DepthStencilState.DepthEnable = true;
    gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    */
    gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    /*
    gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
    gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
    gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();
    */
    //頂点シェーダ、ピクセルシェーダ
    gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
    gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定
    /*
    gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;  // カリングしない
    gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // ポリゴン内塗りつぶし
    gpipeline.RasterizerState.DepthClipEnable = true; // 深度クリッピングを有効に
    */
    //ラスタライザ
    gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    // gpipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;  // RBGA全てのチャンネルを描画

     //レンダ―ターゲットのブレンド設定
    D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
    blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;//標準設定
    blenddesc.BlendEnable = true;//ブレンドを友好にする
    blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;//加算
    blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;//ソースの値を100%使う
    blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;//デストの値を0%使う
    /*加算*/
    //blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//加算
    //blenddesc.SrcBlend = D3D12_BLEND_ONE;//ソースの値を100%使う
    //blenddesc.DestBlend = D3D12_BLEND_ONE;//デストの値を100%使う
    /*減算*/
    //blenddesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;//デストからソースを減算
    //blenddesc.SrcBlend = D3D12_BLEND_ONE;//ソースの値を100%使う
    //blenddesc.DestBlend = D3D12_BLEND_ONE;//デストの値を100%使う
    /*色反転*/
    //blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//加算
    //blenddesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;//デストカラーの値
    //blenddesc.DestBlend = D3D12_BLEND_ZERO;//使わない
    /*半透明*/
    blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//加算
    blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;//ソースのα
    blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;//1.0fのソースのα

    gpipeline.InputLayout.pInputElementDescs = inputLayout;
    gpipeline.InputLayout.NumElements = _countof(inputLayout);

    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    gpipeline.NumRenderTargets = 1; // 描画対象は1つ
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
    gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング

    //スタティックサンプラー
    /*
    D3D12_STATIC_SAMPLER_DESC samplerDesc{};
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    */
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);



    ComPtr<ID3D12RootSignature>rootsignature;

    //
    /*
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.pParameters = rootparams;
    rootSignatureDesc.NumParameters = _countof(rootparams);
    rootSignatureDesc.pStaticSamplers = &samplerDesc;
    rootSignatureDesc.NumStaticSamplers = 1;
    ComPtr<ID3DBlob>rootSigBlob;
    result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_0,
        &rootSigBlob,
        &errorBlob
    );
    result = dev->CreateRootSignature(
        0,
        rootSigBlob->GetBufferPointer(),
        rootSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootsignature)
    );
    //rootSigBlob->Release();
    // パイプラインにルートシグネチャをセット
    gpipeline.pRootSignature = rootsignature.Get();
    */
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


    ComPtr<ID3DBlob>rootSigBlob;
    result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob,
        &errorBlob);

    result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootsignature));

    gpipeline.pRootSignature = rootsignature.Get();

    ComPtr<ID3D12PipelineState>pipelinestate;
    result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelinestate));
    //インデックスデータのサイズ
    UINT sizeIB = static_cast<UINT>(sizeof(unsigned short) * _countof(indices));
    //インデックスバッファの生成
    //
    /*
    ComPtr< ID3D12Resource>indexBuff;
    resdesc.Width = sizeIB;//インデックス情報が入る分のサイズ
    //インデックスバッファの生成
    result = dev->CreateCommittedResource(
        &heapprop,//ヒープ生成
        D3D12_HEAP_FLAG_NONE,
        &resdesc,//リソース設定
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBuff));
        */
        //
    ComPtr< ID3D12Resource>indexBuff;
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeIB),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBuff));

    //GPUのバッファに対応した仮想メモリを取得
    unsigned short* indexMap = nullptr;
    result = indexBuff->Map(0, nullptr, (void**)&indexMap);

    //全イデックスに対して
    for (int i = 0; i < _countof(indices); i++)
    {
        indexMap[i] = indices[i];//インデックスをコピー
    }
    //つながりを解除
    indexBuff->Unmap(0, nullptr);

    D3D12_INDEX_BUFFER_VIEW ibview{};
    ibview.BufferLocation = indexBuff->GetGPUVirtualAddress();
    ibview.Format = DXGI_FORMAT_R16_UINT;
    ibview.SizeInBytes = sizeIB;


    //構造化
    const int constantBufferNum = 300;
    //オブジェクトの数
    const int OBJECT_NUM = 300;
    Object3d object3ds[OBJECT_NUM];

    ComPtr<ID3D12Resource> depthBuffer8;

    CD3DX12_RESOURCE_DESC depthResDesc8 = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT,
        window_width,
        window_height,
        1, 0,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthResDesc8,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0),
        IID_PPV_ARGS(&depthBuffer8)
    );





    //深度ビューでスクリプタヒープ
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc8{};
    dsvHeapDesc8.NumDescriptors = 1;
    dsvHeapDesc8.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ComPtr< ID3D12DescriptorHeap>dsvHeap8;
    result = dev->CreateDescriptorHeap(&dsvHeapDesc8, IID_PPV_ARGS(&dsvHeap8));

    //深度ビュー作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc8 = {};
    dsvDesc8.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc8.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dev->CreateDepthStencilView(
        depthBuffer8.Get(),
        &dsvDesc8,
        dsvHeap8->GetCPUDescriptorHandleForHeapStart()
    );




    XMMATRIX matProjection8 = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(60.0f),
        (float)window_width / window_height,
        0.1f, 1000.0f
    );
    //   constMap8->mat8 = matProjection8;

       //ビュー変換行列
    XMMATRIX matView8;
    XMFLOAT3 eye8(0, 30, -80);
    XMFLOAT3 target8(0, 0, 0);
    XMFLOAT3 up8(0, 1, 0);
    matView8 = XMMatrixLookAtLH(XMLoadFloat3(&eye8), XMLoadFloat3(&target8), XMLoadFloat3(&up8));




    //定数バッファ用
    ID3D12DescriptorHeap* basicDescHeap8;
    //設定構造体
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc8{};
    descHeapDesc8.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descHeapDesc8.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//
    descHeapDesc8.NumDescriptors = constantBufferNum + 1;//シェーダ
    //デスクリフヒープの生成
    result = dev->CreateDescriptorHeap(&descHeapDesc8, IID_PPV_ARGS(&basicDescHeap8));




    for (int i = 0; i < _countof(object3ds); i++)
    {
        InitializeObject3d(&object3ds[i], i, dev.Get(), basicDescHeap8);


        //if (i > 0) {
            //object3ds[i].parent = &object3ds[i - 1];


       

    }


    for (int i = 0; i < _countof(object3ds); i++)
    {
        UpdateObject3d(&object3ds[i], matView8, matProjection8, object3ds[i].Rand_Cr, i, object3ds[i].position.z, 0, 0);
    }

    //でスクリプタヒープの戦闘バンドルを取得
    D3D12_CPU_DESCRIPTOR_HANDLE basicHeapHandle8 =
        basicDescHeap8->GetCPUDescriptorHandleForHeapStart();


    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandleSRV =
        basicDescHeap8->GetCPUDescriptorHandleForHeapStart();

    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandleSRV =
        basicDescHeap8->GetGPUDescriptorHandleForHeapStart();

    cpuDescHandleSRV.ptr += constantBufferNum *
        dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    gpuDescHandleSRV.ptr += constantBufferNum *
        dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);



    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc8{};


    CD3DX12_DESCRIPTOR_RANGE descRangeCBV8, descRangeSRV8;
    descRangeCBV8.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    descRangeSRV8.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER rootparams8[2];
    rootparams8[0].InitAsDescriptorTable(1, &descRangeCBV8);
    rootparams8[1].InitAsDescriptorTable(1, &descRangeSRV8);

    //テクスチャバッファ


    TexMetadata   metadata8{};
    ScratchImage scratchImg8{};

    result = LoadFromWICFile(
        L"Resources/White.png",
        WIC_FLAGS_NONE,
        &metadata8, scratchImg8
    );

    const Image* img8 = scratchImg8.GetImage(0, 0, 0);

    CD3DX12_RESOURCE_DESC texresDesc8 = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata8.format,
        metadata8.width,
        (UINT)metadata8.height,
        (UINT16)metadata8.arraySize,
        (UINT16)metadata8.mipLevels
    );


    ComPtr<ID3D12Resource>texbuff8 = nullptr;
    result = dev->CreateCommittedResource(//GPUリソース設定
        &CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0),
        D3D12_HEAP_FLAG_NONE,
        &texresDesc8,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&texbuff8));

    //


    //でスクリプタ1個分アドレスをまとめる
    basicHeapHandle8.ptr +=
        dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //シェーダリソースビュー設定
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc8{};//設定構造体
    srvDesc8.Format = metadata8.format;//DXGI_FORMAT_R32G32B32A32_FLOAT;//RGBA
    srvDesc8.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc8.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
    srvDesc8.Texture2D.MipLevels = 1;

    //ヒープの2番目にシェーダーリソースビュー作成
    dev->CreateShaderResourceView(texbuff8.Get(),//ビューと関連づけるバッファ
        &srvDesc8,//テクスチャ設定情報
        //basicHeapHandle8
        cpuDescHandleSRV
    );




    //テクスチャバッファにデータ転送
    result = texbuff8->WriteToSubresource(
        0,
        nullptr,//全領域へコピー
        img8->pixels,//元データアドレス
        (UINT)img8->rowPitch,//1ラインサイズ
        (UINT)img8->slicePitch//全サイズ
    );

    //元データ開放


    struct Vertex8
    {
        XMFLOAT3 pos8;//xyz座標
        XMFLOAT3 normal8;//法線ベクトル
        XMFLOAT2 uv8;//uv座標
    };


    //頂点データ
    Vertex8 vertices8[] = {

        {{-10.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},//左下
        {{-10.0f,+5.0f,-5.0f},{}, {0.0f,0.0f}},//左上
        {{+10.0f,-5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        {{+10.0f,+5.0f,-5.0f},{}, {1.0f,0.0f}},//右上
        //後

        {{-10.0f,+5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{-10.0f,-5.0f,5.0f},{}, {0.0f,1.0f}},//左下
        {{+10.0f,+5.0f,5.0f},{}, {1.0f,0.0f}},//右上
        {{+10.0f,-5.0f,5.0f},{}, {1.0f,1.0f}},//右下
        //左
        {{-10.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},//左下
        {{-10.0f,-5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{-10.0f, 5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        {{-10.0f,+5.0f,5.0f},{}, {1.0f,0.0f}},//右上
        //右

        {{10.0f,-5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{10.0f,-5.0f,-5.0f},{}, {0.0f,1.0f}},//左下
        {{10.0f,+5.0f,5.0f},{}, {1.0f,0.0f}},//右上
        {{10.0f, 5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        //下
        {{-10.0f,5.0f,5.0f},{}, {0.0f,1.0f}},//左下
        {{10.0f,5.0f,5.0f},{}, {0.0f,0.0f}},//左上
        {{-10.0f,5.0f,-5.0f},{}, {1.0f,1.0f}},//右下
        {{10.0f,+5.0f,-5.0f},{}, {1.0f,0.0f}},//右上
        //上
        {{-10.0f,-5.0f,5.0f},{}, {0.0f,1.0f}},//左下
        {{-10.0f,-5.0f,-5.0f},{}, {0.0f,0.0f}},//左上
        {{10.0f,-5.0f,5.0f},{}, {1.0f,1.0f}},//右下
        {{10.0f,-5.0f,-5.0f},{}, {1.0f,0.0f}},//右上


    };


    // constMap8->mat8.r[3].m128_f32[0] = -1.0f;
    // constMap8->mat8.r[3].m128_f32[1] = 1.0f;
     //インデックスデータ
    unsigned short indices8[] = {

        0,1,2,//三角形一つ目
        2,1,3,//二つ目
        //
        4,5,6,
        6,5,7,
        //
        8,9,10,
        10,9,11,
        //
        12,13,14,
        14,13,15,
        //
        16,17,18,
        18,17,19,
        //
        20,21,22,
        22,21,23


    };



    for (int i = 0; i < 36 / 3; i++)
    {
        unsigned short indices08 = indices8[i * 3 + 0];
        unsigned short indices18 = indices8[i * 3 + 1];
        unsigned short indices28 = indices8[i * 3 + 2];

        XMVECTOR p08 = XMLoadFloat3(&vertices8[indices08].pos8);
        XMVECTOR p18 = XMLoadFloat3(&vertices8[indices18].pos8);
        XMVECTOR p28 = XMLoadFloat3(&vertices8[indices28].pos8);

        XMVECTOR v18 = XMVectorSubtract(p18, p08);
        XMVECTOR v28 = XMVectorSubtract(p28, p08);

        XMVECTOR normal8 = XMVector3Cross(v18, v28);

        normal8 = XMVector3Normalize(normal8);

        XMStoreFloat3(&vertices8[indices08].normal8, normal8);
        XMStoreFloat3(&vertices8[indices18].normal8, normal8);
        XMStoreFloat3(&vertices8[indices28].normal8, normal8);
    }

    // 頂点データ全体のサイズ = 頂点データ一つ分のサイズ * 頂点データの要素数
    UINT sizeVB8 = static_cast<UINT>(sizeof(Vertex8) * _countof(vertices8));


    ComPtr< ID3D12Resource>vertBuff8;
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeVB8),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertBuff8)
    );

    // GPU上のバッファに対応した仮想メモリを取得
    Vertex8* vertMap8 = nullptr;
    result = vertBuff8->Map(0, nullptr, (void**)&vertMap8);

    // 全頂点に対して
    for (int i = 0; i < _countof(vertices8); i++)
    {
        vertMap8[i] = vertices8[i];   // 座標をコピー
    }

    // マップを解除
    vertBuff8->Unmap(0, nullptr);

    // 頂点バッファビューの作成
    D3D12_VERTEX_BUFFER_VIEW vbView8{};

    vbView8.BufferLocation = vertBuff8->GetGPUVirtualAddress();
    vbView8.SizeInBytes = sizeVB8;
    vbView8.StrideInBytes = sizeof(Vertex8);

    ComPtr<ID3DBlob> vsBlob8; // 頂点シェーダオブジェクト
    ComPtr< ID3DBlob> psBlob8; // ピクセルシェーダオブジェクト
    ComPtr< ID3DBlob> errorBlob8; // エラーオブジェクト

    //インデックスデータのサイズ
    UINT sizeIB8 = static_cast<UINT>(sizeof(unsigned short) * _countof(indices8));
    //インデックスバッファの生成

    ComPtr< ID3D12Resource>indexBuff8;
    result = dev->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeIB8),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBuff8));

    //GPUのバッファに対応した仮想メモリを取得
    unsigned short* indexMap8 = nullptr;
    result = indexBuff8->Map(0, nullptr, (void**)&indexMap8);

    //全イデックスに対して
    for (int i = 0; i < _countof(indices8); i++)
    {
        indexMap8[i] = indices8[i];//インデックスをコピー
    }
    //つながりを解除
    indexBuff8->Unmap(0, nullptr);

    D3D12_INDEX_BUFFER_VIEW ibview8{};
    ibview8.BufferLocation = indexBuff8->GetGPUVirtualAddress();
    ibview8.Format = DXGI_FORMAT_R16_UINT;
    ibview8.SizeInBytes = sizeIB8;

    //パイプライン生成
    PipelineSet object3dPipelineSet = Create3dpipe(dev.Get());
    PipelineSet spritePipelineSet = CreateSprite2dpipe(dev.Get());

    BYTE key[256] = {};
    BYTE olds[256] = {};

    //スプライトサイズ等
    sprite.rotation = { 0,0,0 };
    sprite.position = { 640, 360, 0 };
    sprite.size = { 1280.0f,720.0f };
    sprite.texSize = { 1280.0f,720.0f };

    sprite1.rotation = { 0,0,0 };
    sprite1.position = { 640, 360, 0 };
    sprite1.size = { 1280.0f,720.0f };
    sprite1.texSize = { 1280.0f,720.0f };

    SpriteTransVertexBuffer(sprite, spriteCommon);
    SpriteTransVertexBuffer(sprite1, spriteCommon);

    //ゲーム用変数
    int Flag = 0;

    int score = 0;
    char moji[64];

    int debug_x = 600;
    int debug_y = 80;
   
    float R = 0.07;
    float G = 0.07;
    float B = 0.07;


    //音声読み込み
    SoundData soundData1 = SoundLoadWave("Resources/answer_.wav");//正解の音
    SoundData soundData2 = SoundLoadWave("Resources/failed_.wav");//間違えの音
    SoundData soundData3 = SoundLoadWave("Resources/enter_.wav");//決定音　　　（保留）
    SoundData soundData4 = SoundLoadWave("Resources/move_.wav");//移動音
    SoundData soundData5 = SoundLoadWave("Resources/ruka_.wav");//bgm

    srand(time(NULL));
    SoundPlayWaveLoop(xAudio2.Get(), soundData5);
    while (true)  // ゲームループ
    {
        memcpy(olds, key, sizeof(key));
        ZeroMemory(key, sizeof(key));

        
        if (FAILED(devkeyboard->GetDeviceState(sizeof(key), key)))
        {
            while (devkeyboard->Acquire() == DIERR_INPUTLOST);
        }


        // ブロック内はページ右側を参照
        // メッセージがある？
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); // キー入力メッセージの処理
            DispatchMessage(&msg); // プロシージャにメッセージを送る
        }

        // ✖ボタンで終了メッセージが来たらゲームループを抜ける
        if (msg.message == WM_QUIT) {
            break;
        }


        // DirectX毎フレーム処理　ここから


        //スプライト更新
      /*  SpriteUpdate(sprite, spriteCommon);
        SpriteUpdate(sprite1, spriteCommon);*/

     /*   sprintf_s(moji, "%d", score);*/
        //ゲーム内時間
       /* Time++;*/

        
       /* debugtext.Print(spriteCommon, moji, debug_x, debug_y);
        debugtext2.Print(spriteCommon, moji, debug_x, debug_y, 1.0f);*/


        //画像の回転
        rotation.z = 0.0f;
        rotation.x = 0.0f;
        rotation.y = 0.0f;
        matScale = XMMatrixScaling(scale.x, scale.y, scale.z);
        matTrans = XMMatrixTranslation(position.x, position.y, position.z);
        matRot = XMMatrixIdentity();
        matRot *= XMMatrixRotationZ(XMConvertToRadians(rotation.z));
        matRot *= XMMatrixRotationX(XMConvertToRadians(rotation.x));
        matRot *= XMMatrixRotationY(XMConvertToRadians(rotation.y));

        matWorld = XMMatrixIdentity();
        matWorld *= matScale;
        matWorld *= matRot;
        matWorld *= matTrans;

        // GPU上のバッファに対応した仮想メモリを取得
        Vertex* vertMap = nullptr;
        result = vertBuff->Map(0, nullptr, (void**)&vertMap);
        constMap->mat = matWorld * matView * matProjection;
        // 全頂点に対して
        for (int i = 0; i < _countof(vertices); i++)
        {
            vertMap[i] = vertices[i];   // 座標をコピー
        }
        constMap->mat = matWorld * matView * matProjection;
        // マップを解除
        vertBuff->Unmap(0, nullptr);


        char str[128];
        //  sprintf_s(str, "x=%.2f,y=%.2f",x,-y);
        OutputDebugStringA(str);
        OutputDebugStringA("\n");

        // バックバッファの番号を取得（2つなので0番か1番）
        UINT bbIndex = swapchain->GetCurrentBackBufferIndex();

        // １．リソースバリアで書き込み可能に変更
        //
        /*
        D3D12_RESOURCE_BARRIER barrierDesc{};
        barrierDesc.Transition.pResource = backBuffers[bbIndex].Get(); // バックバッファを指定
        barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 表/ 示から
        barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画
        cmdList->ResourceBarrier(1, &barrierDesc);
        */
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        // ２．描画先指定
        // レンダーターゲットビュー用ディスクリプタヒープのハンドルを取得
        D3D12_CPU_DESCRIPTOR_HANDLE rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
        rtvH.ptr += bbIndex * dev->GetDescriptorHandleIncrementSize(heapDesc.Type);
        // cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);
        D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

        // ３．画面クリア           R     G     B    A
        float clearColor[] = { R,G, B,0 }; // 青っぽい色
        cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
        cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        cmdList->IASetIndexBuffer(&ibview);
        // cmdList->IASetIndexBuffer(&ibview2);
         // ４．描画コマンドここから

    

        //自機
        //cmdList->DrawInstanced(3, 1, 0, 0);


        //描画
        cmdList->SetPipelineState(pipelinestate.Get());
        cmdList->SetGraphicsRootSignature(rootsignature.Get());
        //デスクリプタヒープをセット
        ID3D12DescriptorHeap* ppHeaps[] = { basicDescHeap.Get() };
        cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        //定数バッファビューをセット
      //  cmdList->SetGraphicsRootDescriptorTable(0, basicdescHeap->GetGPUDescriptorHandleForHeapStart());
        //デスクリプタヒープの先頭ハンドルを取得
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle = basicDescHeap->GetGPUDescriptorHandleForHeapStart();
        //ヒープの先頭にあるCBVを√パラメータ0番に設定
        cmdList->SetGraphicsRootDescriptorTable(0, gpuDescHandle);
        //ハンドル江尾1つ進める
        gpuDescHandle.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //ヒープの2番目にあるSRVを√パラメータ1番に設定
        cmdList->SetGraphicsRootDescriptorTable(1, gpuDescHandle);


        D3D12_VIEWPORT viewport{};

        viewport.Width = window_width;
        viewport.Height = window_height;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        cmdList->RSSetViewports(1, &viewport);

        D3D12_RECT scissorrect{};

        scissorrect.left = 0;                                       // 切り抜き座標左
        scissorrect.right = scissorrect.left + window_width;        // 切り抜き座標右
        scissorrect.top = 0;                                        // 切り抜き座標上
        scissorrect.bottom = scissorrect.top + window_height;       // 切り抜き座標下

        cmdList->RSSetScissorRects(1, &scissorrect);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmdList->IASetVertexBuffers(0, 1, &vbView);

        cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);


        for (int i = 0; i < _countof(object3ds); i++)
        {
            UpdateObject3d(&object3ds[i], matView8, matProjection8, object3ds[i].Rand_Cr, i, object3ds[i].position.z, 0, 0);
        }




        Vertex8* vertMap8 = nullptr;
        result = vertBuff8->Map(0, nullptr, (void**)&vertMap8);
        //constMap8->mat8 = matWorld8 * matView8 * matProjection8;
        // 全頂点に対して
        for (int i = 0; i < _countof(vertices8); i++)
        {
            vertMap8[i] = vertices8[i];   // 座標をコピー
        }
        //constMap9->mat9 = matWorld9 * matView9 * matProjection9;
        // マップを解除
        vertBuff8->Unmap(0, nullptr);


        //8つ目
        cmdList->DrawInstanced(3, 1, 0, 0);
        //constMap9->color9 = XMFLOAT4(r, g, b, 1);//半透明の赤
        cmdList->SetPipelineState(object3dPipelineSet.pipelinestate.Get());
        cmdList->SetGraphicsRootSignature(object3dPipelineSet.rootsignature.Get());

        //デスクリプタヒープの先頭ハンドルを取得
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle8 = basicDescHeap8->GetGPUDescriptorHandleForHeapStart();
        //ヒープの先頭にあるCBVを√パラメータ0番に設定
        //ハンドル江尾1つ進める
        gpuDescHandle8.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_VIEWPORT viewport8{};

        viewport8.Width = window_width;
        viewport8.Height = window_height;
        viewport8.TopLeftX = 0;
        viewport8.TopLeftY = 0;
        viewport8.MinDepth = 0.0f;
        viewport8.MaxDepth = 1.0f;

        cmdList->RSSetViewports(1, &viewport8);

        D3D12_RECT scissorrect8{};

        scissorrect8.left = 0;                                       // 切り抜き座標左
        scissorrect8.right = scissorrect8.left + window_width;        // 切り抜き座標右
        scissorrect8.top = 0;                                        // 切り抜き座標上
        scissorrect8.bottom = scissorrect8.top + window_height;       // 切り抜き座標下

        cmdList->RSSetScissorRects(1, &scissorrect8);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


        for (int i = 0; i < _countof(object3ds); i++)
        {
            if (object3ds[i].Flag == 1 && i < 200)
            {
                DrawObject3d(&object3ds[i], cmdList.Get(), basicDescHeap8, vbView8, ibview8, gpuDescHandleSRV, _countof(indices8));
            }
        }



        ////スプライト共通コマンド
        //SpriteCommonBeginDraw(cmdList.Get(), spriteCommon);
      
        //スプライト描画
        //if (GameScene == 0)
        //{
        //    SpriteDraw(sprite, cmdList.Get(), spriteCommon, dev.Get());//title
        //    
        //}
        //if (GameScene == 1)
        //{
        //    debugtext.DrawAll(cmdList.Get(), spriteCommon, dev.Get());
        //}
        //if (GameScene == 2)
        //{
   
        //    SpriteDraw(sprite1, cmdList.Get(), spriteCommon, dev.Get());//gameover

        //   
        //    debugtext2.DrawAll(cmdList.Get(), spriteCommon, dev.Get());
        //}
        

        // ４．描画コマンドここまで


        // ５．リソースバリアを戻す
        //barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; // 描画
        //barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;   // 表示に
        //cmdList->ResourceBarrier(1, &barrierDesc);
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[bbIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        // 命令のクローズ
        cmdList->Close();
        // コマンドリストの実行
        ID3D12CommandList* cmdLists[] = { cmdList.Get() }; // コマンドリストの配列
        cmdQueue->ExecuteCommandLists(1, cmdLists);
        // コマンドリストの実行完了を待つ
        cmdQueue->Signal(fence.Get(), ++fenceVal);
        if (fence->GetCompletedValue() != fenceVal) {
            HANDLE event = CreateEvent(nullptr, false, false, nullptr);
            fence->SetEventOnCompletion(fenceVal, event);
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        }

        cmdAllocator->Reset(); // キューをクリア
        cmdList->Reset(cmdAllocator.Get(), nullptr);  // 再びコマンドリストを貯める準備

        // バッファをフリップ（裏表の入替え）
        swapchain->Present(1, 0);

        // DirectX毎フレーム処理　ここまで

    }
    // ウィンドウクラスを登録解除
    UnregisterClass(w.lpszClassName, w.hInstance);

    xAudio2.Reset();
    SoundUnload(&soundData1);
    SoundUnload(&soundData2);
    SoundUnload(&soundData4);
    return 0;
}
/*
PipeLineSet object3dPipelineSet(ID3D12Device* dev)
{
    HRESULT result;
    ComPtr<ID3DBlob> vsBlob; // 頂点シェーダオブジェクト
    ComPtr< ID3DBlob> psBlob; // ピクセルシェーダオブジェクト
    ComPtr< ID3DBlob> errorBlob; // エラーオブジェクト
    //ピクセルの読み込みコンパイル
    result = D3DCompileFromFile(
        L"BasicPS.hlsl",   // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "ps_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &psBlob, &errorBlob);
    if (FAILED(result)) {
        // errorBlobからエラー内容をstring型にコピー
        std::string errstr;
        errstr.resize(errorBlob->GetBufferSize());
        std::copy_n((char*)errorBlob->GetBufferPointer(),
            errorBlob->GetBufferSize(),
            errstr.begin());
        errstr += "\n";
        // エラー内容を出力ウィンドウに表示
        OutputDebugStringA(errstr.c_str());
        exit(1);
    }
    // 頂点シェーダの読み込みとコンパイル
    result = D3DCompileFromFile(
        L"BasicVS.hlsl",  // シェーダファイル名
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルード可能にする
        "main", "vs_5_0", // エントリーポイント名、シェーダーモデル指定
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用設定
        0,
        &vsBlob, &errorBlob);
    // 頂点レイアウト
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }, // (1行で書いたほうが見やすい)
        {
            "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
        },
        {
            "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
        },
    };
    // グラフィックスパイプライン設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline{};

    gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    //頂点シェーダ、ピクセルシェーダ
    gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
    gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
    gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // 標準設定

    //ラスタライザ
    gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
     //レンダ―ターゲットのブレンド設定
    D3D12_RENDER_TARGET_BLEND_DESC& blenddesc = gpipeline.BlendState.RenderTarget[0];
    blenddesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;//標準設定
    blenddesc.BlendEnable = true;//ブレンドを友好にする
    blenddesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;//加算
    blenddesc.SrcBlendAlpha = D3D12_BLEND_ONE;//ソースの値を100%使う
    blenddesc.DestBlendAlpha = D3D12_BLEND_ZERO;//デストの値を0%使う
    /*加算*/
    //blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//加算
    //blenddesc.SrcBlend = D3D12_BLEND_ONE;//ソースの値を100%使う
    //blenddesc.DestBlend = D3D12_BLEND_ONE;//デストの値を100%使う
    /*減算*/
    //blenddesc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;//デストからソースを減算
    //blenddesc.SrcBlend = D3D12_BLEND_ONE;//ソースの値を100%使う
    //blenddesc.DestBlend = D3D12_BLEND_ONE;//デストの値を100%使う
    /*色反転*/
    //blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//加算
    //blenddesc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;//デストカラーの値
    //blenddesc.DestBlend = D3D12_BLEND_ZERO;//使わない
    /*半透明*/
/*
    blenddesc.BlendOp = D3D12_BLEND_OP_ADD;//加算
    blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;//ソースのα
    blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;//1.0fのソースのα
    gpipeline.InputLayout.pInputElementDescs = inputLayout;
    gpipeline.InputLayout.NumElements = _countof(inputLayout);
    gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gpipeline.NumRenderTargets = 1; // 描画対象は1つ
    gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～255指定のRGBA
    gpipeline.SampleDesc.Count = 1; // 1ピクセルにつき1回サンプリング
    //スタティックサンプラー

    CD3DX12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);
    PipeLineSet pipelineSet;
    ComPtr<ID3D12RootSignature>rootsignature;
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_0(_countof(rootparams), rootparams, 1, &samplerDesc,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob>rootSigBlob;
    result = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob,
        &errorBlob);
    result = dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootsignature));
    gpipeline.pRootSignature = rootsignature.Get();
    ComPtr<ID3D12PipelineState>pipelinestate;
    result = dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelinestate));
    return pipelineSet;
}*/


void InitializeObject3d(Object3d* object, int index, ComPtr<ID3D12Device> dev,
    ID3D12DescriptorHeap* descHeap)
{
    //定数バッファデータ構造体
   // struct ConstBufferData8 {
   //     XMFLOAT4 color8;//色
   //     XMMATRIX mat8;//3D変換行列
   // };

    HRESULT result;

    D3D12_HEAP_PROPERTIES heapprop{};
    heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resdesc{};
    resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resdesc.Width = (sizeof(ConstBufferData8) + 0xff) & ~0xff;
    resdesc.Height = 1;
    resdesc.DepthOrArraySize = 1;
    resdesc.MipLevels = 1;
    resdesc.SampleDesc.Count = 1;
    resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    result = dev->CreateCommittedResource(
        &heapprop,
        D3D12_HEAP_FLAG_NONE,
        &resdesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&object->constBuff)
    );

    UINT descHandleIncrementSize =
        dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    object->cpuDescHandleCBV = descHeap->GetCPUDescriptorHandleForHeapStart();
    object->cpuDescHandleCBV.ptr += index * descHandleIncrementSize;

    object->gpuDescHandleCBV = descHeap->GetGPUDescriptorHandleForHeapStart();
    object->gpuDescHandleCBV.ptr += index * descHandleIncrementSize;


    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = object->constBuff->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = (UINT)object->constBuff->GetDesc().Width;
    dev->CreateConstantBufferView(&cbvDesc, object->cpuDescHandleCBV);




}

void UpdateObject3d(Object3d* object, XMMATRIX& matView, XMMATRIX& matProjection, int Cr, int i, float rangeR, float rangeW, float rangeG)
{
    XMMATRIX matScale, matRot, matTrans;

    matScale = XMMatrixScaling(object->scale.x, object->scale.y, object->scale.z);
    matRot = XMMatrixIdentity();
    matRot *= XMMatrixRotationZ(XMConvertToRadians(object->rotation.z));
    matRot *= XMMatrixRotationX(XMConvertToRadians(object->rotation.x));
    matRot *= XMMatrixRotationY(XMConvertToRadians(object->rotation.y));
    matTrans = XMMatrixTranslation(
        object->position.x, object->position.y, object->position.z
    );


    object->matWorld = XMMatrixIdentity();
    object->matWorld *= matScale;
    object->matWorld *= matRot;
    object->matWorld *= matTrans;

    if (object->parent != nullptr)
    {
        object->matWorld *= object->parent->matWorld;
    }

    ConstBufferData8* constMap = nullptr;
    if (SUCCEEDED(object->constBuff->Map(0, nullptr, (void**)&constMap))) {

        float Color = 0;
        int Flag = 0;

        for (int i = 0; i < 10; i++)
        {
            if (rangeR <= i * 57.0f)
            {
                Color += 0.05f;
            }
        }

        if (i < 100)
        {
            constMap->color8 = XMFLOAT4(Color, Color, Color, 0.3);//レーン
        }


        if (Cr == 1 || i >= 100)
        {
            constMap->color8 = XMFLOAT4(1, 1, 1, 1);//障害物白
        }


        if (Cr != 1 && i >= 100)
        {
            constMap->color8 = XMFLOAT4(0, 0, 0, 1);//障害物黒
        }

        constMap->mat8 = object->matWorld * matView * matProjection;
        object->constBuff->Unmap(0, nullptr);
    }

}

void DrawObject3d(Object3d* object, ID3D12GraphicsCommandList* cmdList,
    ID3D12DescriptorHeap* descHeap, D3D12_VERTEX_BUFFER_VIEW& vbView,
    D3D12_INDEX_BUFFER_VIEW& ibView, D3D12_GPU_DESCRIPTOR_HANDLE
    gpuDescHandleSRV, UINT numIndices)
{
    cmdList->IASetVertexBuffers(0, 1, &vbView);

    cmdList->IASetIndexBuffer(&ibView);

    ID3D12DescriptorHeap* ppHeaps[] = { descHeap };
    cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    cmdList->SetGraphicsRootConstantBufferView(0, object->constBuff->GetGPUVirtualAddress());

    cmdList->SetGraphicsRootDescriptorTable(1, gpuDescHandleSRV);

    cmdList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);


}
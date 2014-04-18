//***************************************************************************************
// TerrainDemo.cpp by Frank Luna (C) 2011 All Rights Reserved.
//
// Demonstrates terrain rendering using tessellation and multitexturing.
//
// Controls:
//		Hold the left mouse button down and move the mouse to rotate.
//      Hold the right mouse button down to zoom in and out.
//
//***************************************************************************************

#include "d3dApp.h"
#include "d3dx11Effect.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "LightHelper.h"
#include "Effects.h"
#include "Vertex.h"
#include "RenderStates.h"
#include "Camera.h"
#include "Sky.h"
#include "Terrain.h"
#include "Waves.h"
 
class TerrainApp : public D3DApp
{
public:
	TerrainApp(HINSTANCE hInstance);
	~TerrainApp();

	bool Init();
	void OnResize();
	void UpdateScene(float dt);
	void DrawScene(); 

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

private:
	void DrawScene(const Camera& camera, bool water);
	void BuildCubeFaceCamera(float x, float y, float z);
	void BuildDynamicCubeMapViews();
	void BuildWaveGeometryBuffers();
	ID3D11DepthStencilView* mDynamicCubeMapDSV;
	ID3D11RenderTargetView* mDynamicCubeMapRTV[6];
	ID3D11ShaderResourceView* mDynamicCubeMapSRV;
	D3D11_VIEWPORT mCubeMapViewport;
	static const int CubeMapSize = 256;
	Camera mCubeMapCamera[6];

	ID3D11Buffer* mWavesVB;
	ID3D11Buffer* mWavesIB;
	ID3D11ShaderResourceView* mWavesMapSRV;
	Waves mWaves;
	Material mWavesMat;
	XMFLOAT4X4 mWaterTexTransform;
	XMFLOAT4X4 mWavesWorld;
	XMFLOAT2 mWaterTexOffset;

	Sky* mSky;
	Terrain mTerrain;

	DirectionalLight mDirLights[3];

	Camera mCam;

	bool mWalkCamMode;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	TerrainApp theApp(hInstance);
	
	if( !theApp.Init() )
		return 0;
	
	return theApp.Run();
}

TerrainApp::TerrainApp(HINSTANCE hInstance)
: D3DApp(hInstance), mSky(0), mWalkCamMode(false), mWavesVB(0), mWavesIB(0), mWavesMapSRV(0),
  mWaterTexOffset(0.0f, 0.0f), mDynamicCubeMapDSV(0), mDynamicCubeMapSRV(0)
{
	mMainWndCaption = L"Terrain Demo";
	mEnable4xMsaa = false;

	mLastMousePos.x = 0;
	mLastMousePos.y = 0;



	mCam.SetPosition(0.0f, 10.0f, 100.0f);
	BuildCubeFaceCamera(0.0f, 5.0f, 0.0f);

	
	for(int i = 0; i < 6; ++i)
	{
		mDynamicCubeMapRTV[i] = 0;
	}
	XMMATRIX I = XMMatrixIdentity();
	XMStoreFloat4x4(&mWavesWorld, I);

	mDirLights[0].Ambient  = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	mDirLights[0].Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mDirLights[0].Specular = XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f);
	mDirLights[0].Direction = XMFLOAT3(0.707f, -0.707f, 0.0f);

	mDirLights[1].Ambient  = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mDirLights[1].Diffuse  = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDirLights[1].Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDirLights[1].Direction = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

	mDirLights[2].Ambient  = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mDirLights[2].Diffuse  = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDirLights[2].Specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDirLights[2].Direction = XMFLOAT3(-0.57735f, -0.57735f, -0.57735f);
	
	mWavesMat.Ambient  = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mWavesMat.Diffuse  = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.5f);
	mWavesMat.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 32.0f);
	mWavesMat.Reflect  = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
}

TerrainApp::~TerrainApp()
{
	md3dImmediateContext->ClearState();
	
	SafeDelete(mSky);

	ReleaseCOM(mWavesVB);
	ReleaseCOM(mWavesIB);
	ReleaseCOM(mWavesMapSRV);
	ReleaseCOM(mDynamicCubeMapDSV);
	ReleaseCOM(mDynamicCubeMapSRV);
	for(int i = 0; i < 6; ++i)
		ReleaseCOM(mDynamicCubeMapRTV[i]);

	Effects::DestroyAll();
	InputLayouts::DestroyAll();
	RenderStates::DestroyAll();
}

bool TerrainApp::Init()
{
	if(!D3DApp::Init())
		return false;

	mWaves.Init(160, 160, 1.0f, 0.03f, 5.0f, 0.3f);

	// Must init Effects first since InputLayouts depend on shader signatures.
	Effects::InitAll(md3dDevice);
	InputLayouts::InitAll(md3dDevice);
	RenderStates::InitAll(md3dDevice);

	mSky = new Sky(md3dDevice, L"Textures/grasscube1024.dds", 5000.0f);

	Terrain::InitInfo tii;
	tii.HeightMapFilename = L"Textures/terrain.raw";
	tii.LayerMapFilename0 = L"Textures/grass.dds";
	tii.LayerMapFilename1 = L"Textures/darkdirt.dds";
	tii.LayerMapFilename2 = L"Textures/stone.dds";
	tii.LayerMapFilename3 = L"Textures/lightdirt.dds";
	tii.LayerMapFilename4 = L"Textures/snow.dds";
	tii.BlendMapFilename = L"Textures/blend.dds";
	tii.HeightScale = 50.0f;
	tii.HeightmapWidth = 2049;
	tii.HeightmapHeight = 2049;
	tii.CellSpacing = 0.5f;

	HR(D3DX11CreateShaderResourceViewFromFile(md3dDevice, 
		L"Textures/water2.dds", 0, 0, &mWavesMapSRV, 0 ));

	BuildDynamicCubeMapViews();
	BuildWaveGeometryBuffers();
	mTerrain.Init(md3dDevice, md3dImmediateContext, tii);
	return true;
}

void TerrainApp::OnResize()
{
	D3DApp::OnResize();

	mCam.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 3000.0f);
}

void TerrainApp::UpdateScene(float dt)
{
	//
	// Control the camera.
	//
	if( GetAsyncKeyState('W') & 0x8000 )
		mCam.Walk(10.0f*dt);

	if( GetAsyncKeyState('S') & 0x8000 )
		mCam.Walk(-10.0f*dt);

	if( GetAsyncKeyState('A') & 0x8000 )
		mCam.Strafe(-10.0f*dt);

	if( GetAsyncKeyState('D') & 0x8000 )
		mCam.Strafe(10.0f*dt);

	//
	// Walk/fly mode
	//
	if( GetAsyncKeyState('2') & 0x8000 )
		mWalkCamMode = true;
	if( GetAsyncKeyState('3') & 0x8000 )
		mWalkCamMode = false;

	// 
	// Clamp camera to terrain surface in walk mode.
	//
	if( mWalkCamMode )
	{
		XMFLOAT3 camPos = mCam.GetPosition();
		float y = mTerrain.GetHeight(camPos.x, camPos.z);
		mCam.SetPosition(camPos.x, y + 2.0f, camPos.z);
	}

	static float t_base = 0.0f;
	if( (mTimer.TotalTime() - t_base) >= 0.1f )
	{
		t_base += 0.1f;
 
		DWORD i = 5 + rand() % (mWaves.RowCount()-10);
		DWORD j = 5 + rand() % (mWaves.ColumnCount()-10);

		float r = MathHelper::RandF(0.5f, 1.0f);

		mWaves.Disturb(i, j, r);
	}

	mWaves.Update(dt);

	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR(md3dImmediateContext->Map(mWavesVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));

	Vertex::Basic32* v = reinterpret_cast<Vertex::Basic32*>(mappedData.pData);
	for(UINT i = 0; i < mWaves.VertexCount(); ++i)
	{
		v[i].Pos    = mWaves[i];
		v[i].Normal = mWaves.Normal(i);

		// Derive tex-coords in [0,1] from position.
		v[i].Tex.x  = 0.5f + mWaves[i].x / mWaves.Width();
		v[i].Tex.y  = 0.5f - mWaves[i].z / mWaves.Depth();
	}

	md3dImmediateContext->Unmap(mWavesVB, 0);

	XMMATRIX wavesScale = XMMatrixScaling(5.0f, 5.0f, 0.0f);

	mWaterTexOffset.y += 0.05f*dt;
	mWaterTexOffset.x += 0.1f*dt;	
	XMMATRIX wavesOffset = XMMatrixTranslation(mWaterTexOffset.x, mWaterTexOffset.y, 0.0f);

	XMStoreFloat4x4(&mWaterTexTransform, wavesScale*wavesOffset);

	mCam.UpdateViewMatrix();
}

void TerrainApp::DrawScene()
{
	ID3D11RenderTargetView* renderTargets[1];

	// Generate the cube map.
	//RL Set the viewport of the immediate context to the cube map viewport
	md3dImmediateContext->RSSetViewports(1, &mCubeMapViewport);

	
	
	//RL Bind the cube map as the render target
	for(int i = 0; i < 6; ++i)
	{
		// Clear cube map face and depth buffer.
		//RL Clear each RTV (HINT: reinterpret_cast<const float*>(&Colors::Silver) is important)
		//RL Clear the Depth Stencil View (HINT: D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL is important)
		md3dImmediateContext->ClearRenderTargetView(mDynamicCubeMapRTV[i], reinterpret_cast<const float*>(&Colors::Silver));
		md3dImmediateContext->ClearDepthStencilView(mDynamicCubeMapDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// Bind cube map face as render target.
		renderTargets[0] = mDynamicCubeMapRTV[i];
		md3dImmediateContext->OMSetRenderTargets(1, renderTargets, mDynamicCubeMapDSV);

		// Draw the scene with the exception of the center sphere to this cube map face.
		//RL Draw the scene using the overloaded function DrawScene (mCubeMapCamera is important here)
		DrawScene(mCubeMapCamera[i], false);
	}

	// Restore old viewport and render targets.
    md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
	renderTargets[0] = mRenderTargetView;
    md3dImmediateContext->OMSetRenderTargets(1, renderTargets, mDepthStencilView);

    // Have hardware generate lower mipmap levels of cube map.
    md3dImmediateContext->GenerateMips(mDynamicCubeMapSRV);
	
	// Now draw the scene as normal, but with the center sphere.
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::Silver));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
	
	DrawScene(mCam, true);

	HR(mSwapChain->Present(0, 0));
}

void TerrainApp::DrawScene(const Camera& camera, bool water)
{
	

	md3dImmediateContext->IASetInputLayout(InputLayouts::Basic32);
    md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 
	float blendFactor[] = {0.0f, 0.0f, 0.0f, 0.0f};



	UINT stride = sizeof(Vertex::Basic32);
    UINT offset = 0;

	// Set per frame constants.
	Effects::BasicFX->SetDirLights(mDirLights);
	Effects::BasicFX->SetEyePosW(mCam.GetPosition());

	ID3DX11EffectTechnique* landAndWavesTech;
	landAndWavesTech = Effects::BasicFX->Light3ReflectTech;

	D3DX11_TECHNIQUE_DESC techDesc;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &mWavesVB, &stride, &offset);
	md3dImmediateContext->IASetIndexBuffer(mWavesIB, DXGI_FORMAT_R32_UINT, 0);
	if(water)
		{
		landAndWavesTech->GetDesc( &techDesc );
		for(UINT p = 0; p < techDesc.Passes; ++p)
		{
			//
			// Draw the waves.
			//
			md3dImmediateContext->IASetVertexBuffers(0, 1, &mWavesVB, &stride, &offset);
			md3dImmediateContext->IASetIndexBuffer(mWavesIB, DXGI_FORMAT_R32_UINT, 0);

			// Set per object constants.
			XMMATRIX world = XMLoadFloat4x4(&mWavesWorld);
			XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
			XMMATRIX worldViewProj = world*/*mCam*/camera.View()*/*mCam*/camera.Proj();
		
			Effects::BasicFX->SetWorld(world);
			Effects::BasicFX->SetWorldInvTranspose(worldInvTranspose);
			Effects::BasicFX->SetWorldViewProj(worldViewProj);
			Effects::BasicFX->SetTexTransform(XMLoadFloat4x4(&mWaterTexTransform));
			Effects::BasicFX->SetMaterial(mWavesMat);
			Effects::BasicFX->SetDiffuseMap(mWavesMapSRV);
			Effects::BasicFX->SetCubeMap(mDynamicCubeMapSRV);
		
			//RL Tell the context what blend state to use (HINT: OMSetBlendState)
			md3dImmediateContext->OMSetBlendState(RenderStates::TransparentBS ,blendFactor, 0xffffffff);

			landAndWavesTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->DrawIndexed(3*mWaves.TriangleCount(), 0, 0);

			// Restore default blend state
			md3dImmediateContext->OMSetBlendState(0, blendFactor, 0xffffffff);
		}
	}


	mTerrain.Draw(md3dImmediateContext, camera, mDirLights);
	

	// restore default states, as the SkyFX changes them in the effect file.
	
	mSky->Draw(md3dImmediateContext, camera);
	//if( GetAsyncKeyState('1') & 0x8000 )
		//md3dImmediateContext->RSSetState(RenderStates::WireframeRS);

	
	md3dImmediateContext->RSSetState(0);
	md3dImmediateContext->OMSetDepthStencilState(0, 0);
	HR(mSwapChain->Present(0, 0));
}

void TerrainApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void TerrainApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void TerrainApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if( (btnState & MK_LBUTTON) != 0 )
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		mCam.Pitch(dy);
		mCam.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}
void TerrainApp::BuildWaveGeometryBuffers()
{
	// Create the vertex buffer.  Note that we allocate space only, as
	// we will be updating the data every time step of the simulation.

    D3D11_BUFFER_DESC vbd;
    vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(Vertex::Basic32) * mWaves.VertexCount();
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbd.MiscFlags = 0;
    HR(md3dDevice->CreateBuffer(&vbd, 0, &mWavesVB));


	// Create the index buffer.  The index buffer is fixed, so we only 
	// need to create and set once.

	std::vector<UINT> indices(3*mWaves.TriangleCount()); // 3 indices per face

	// Iterate over each quad.
	UINT m = mWaves.RowCount();
	UINT n = mWaves.ColumnCount();
	int k = 0;
	for(UINT i = 0; i < m-1; ++i)
	{
		for(DWORD j = 0; j < n-1; ++j)
		{
			indices[k]   = i*n+j;
			indices[k+1] = i*n+j+1;
			indices[k+2] = (i+1)*n+j;

			indices[k+3] = (i+1)*n+j;
			indices[k+4] = i*n+j+1;
			indices[k+5] = (i+1)*n+j+1;

			k += 6; // next quad
		}
	}

	D3D11_BUFFER_DESC ibd;
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(UINT) * indices.size();
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;
    ibd.MiscFlags = 0;
    D3D11_SUBRESOURCE_DATA iinitData;
    iinitData.pSysMem = &indices[0];
    HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &mWavesIB));
}

void TerrainApp::BuildCubeFaceCamera(float x, float y, float z)
{
	// Generate the cube map about the given position.
	XMFLOAT3 center(x, y, z);
	XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);

	// Look along each coordinate axis.
	XMFLOAT3 targets[6] = 
	{
		XMFLOAT3(x+1.0f, y, z), // +X
		XMFLOAT3(x-1.0f, y, z), // -X
		XMFLOAT3(x, y+1.0f, z), // +Y
		XMFLOAT3(x, y-1.0f, z), // -Y
		XMFLOAT3(x, y, z+1.0f), // +Z
		XMFLOAT3(x, y, z-1.0f)  // -Z
	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	XMFLOAT3 ups[6] = 
	{
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
		XMFLOAT3(0.0f, 1.0f, 0.0f),	 // +Z
		XMFLOAT3(0.0f, 1.0f, 0.0f)	 // -Z
	};

	for(int i = 0; i < 6; ++i)
	{
		mCubeMapCamera[i].LookAt(center, targets[i], ups[i]);
		mCubeMapCamera[i].SetLens(0.5f*XM_PI, 1.0f, 0.1f, 1000.0f);
		mCubeMapCamera[i].UpdateViewMatrix();
	}
}

void TerrainApp::BuildDynamicCubeMapViews()
{
	//
	// Cubemap is a special texture array with 6 elements.
	//
    
	//RL Describe the Texture Array
	//RL Requires use of D3D11_TEXTURE2D_DESC
	//RL HINT: CubeMapSize is a variable that is already defined
	//RL HINT: D3D11_TEXTURE2D_DESC has a property ArraySize
	//RL HINT: Format = DXGI_FORMAT_R8G8B8A8_UNORM
	//RL HINT: BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
	//RL HINT: Don't forget the MiscFlags.
	
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = CubeMapSize;
	texDesc.Height = CubeMapSize;
	texDesc.MipLevels = 0;
	texDesc.ArraySize =6;
	texDesc.SampleDesc.Count =1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage=D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS |
	D3D11_RESOURCE_MISC_TEXTURECUBE;
	
	//RL Create the Texture2D using cubeTex
	ID3D11Texture2D* cubeTex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &cubeTex));
	 //
	 // Create a render target view to each cube map face 
	 // (i.e., each element in the texture array).
	 // 

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = texDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.ArraySize = 1;
    rtvDesc.Texture2DArray.MipSlice = 0;

    for(int i = 0; i < 6; ++i)
    {
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        HR(md3dDevice->CreateRenderTargetView(cubeTex, &rtvDesc, &mDynamicCubeMapRTV[i]));
    }

	//
    // Create a shader resource view to the cube map.
	//

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;

    HR(md3dDevice->CreateShaderResourceView(cubeTex, &srvDesc, &mDynamicCubeMapSRV));

	ReleaseCOM(cubeTex);

	//
	// We need a depth texture for rendering the scene into the cubemap
	// that has the same resolution as the cubemap faces.  
	//

	//RL Create the depth texture
	//RL Requires use of D3D11_TEXTURE2D_DESC
	//RL HINT: CubeMapSize is important again
	//RL HINT: BindFlags = D3D11_BIND_DEPTH_STENCIL
	//RL HINT: Format = DXGI_FORMAT_D32_FLOAT;
	//ID3D11DepthStencilView* mDynamicCubeMapDSV;

	D3D11_TEXTURE2D_DESC depthTexDesc;
    depthTexDesc.Width = CubeMapSize;
    depthTexDesc.Height = CubeMapSize;
    depthTexDesc.MipLevels = 1;
    depthTexDesc.ArraySize = 1;
    depthTexDesc.SampleDesc.Count = 1;
    depthTexDesc.SampleDesc.Quality = 0;
    depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
    depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthTexDesc.CPUAccessFlags = 0;
    depthTexDesc.MiscFlags = 0;

	//RL Create the depth texture using depthTex 

	ID3D11Texture2D* depthTex = 0;
	HR(md3dDevice->CreateTexture2D(&depthTexDesc, 0, &depthTex));

    // Create the depth stencil view for the entire cube
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format = depthTexDesc.Format;
	dsvDesc.Flags  = 0;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    HR(md3dDevice->CreateDepthStencilView(depthTex, &dsvDesc, &mDynamicCubeMapDSV));

	ReleaseCOM(depthTex);

	//
	// Viewport for drawing into cubemap.
	// 

	//RL Set the values for the cube map viewport
	//RL Again, CubeMapSize is important
	//D3D11_VIEWPORT mCubeMapViewport;
	mCubeMapViewport.TopLeftX = 0.0f;
	mCubeMapViewport.TopLeftY = 0.0f;
	mCubeMapViewport.Width = (float)CubeMapSize;
	mCubeMapViewport.Height = (float)CubeMapSize;
	mCubeMapViewport.MinDepth = 0.0f;
	mCubeMapViewport.MaxDepth = 1.0f;
}

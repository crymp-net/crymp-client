#include "StdAfx.h"
#include "BudgetingSystem.h"
#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySoundSystem/ISound.h"
#include "CryCommon/CryRenderer/IRenderAuxGeom.h"
#include "CryCommon/CrySystem/ITimer.h"
#include "CryCommon/CrySystem/IConsole.h"


extern int CryMemoryGetAllocatedSize();


const int c_yStepSizeText( 18 );
const int c_yStepSizeTextMeter( 8 );
const float c_fontScale( 1.3f );

const char c_sys_budget_sysmem[] = "sys_budget_sysmem";
const char c_sys_budget_videomem[] = "sys_budget_videomem";
const char c_sys_budget_frametime[] = "sys_budget_frametime";
const char c_sys_budget_soundchannels[] = "sys_budget_soundchannels";
const char c_sys_budget_soundmem[] = "sys_budget_soundmem";
const char c_sys_budget_numdrawcalls[] = "sys_budget_numdrawcalls";


CBudgetingSystem::CBudgetingSystem()
: m_pRenderer( 0 )
, m_pAuxRenderer( 0 )
, m_pTimer( 0 )
, m_pISoundSystem ( 0 )
, m_sysMemLimitInMB( 512 )
, m_videoMemLimitInMB( 256 )
, m_frameTimeLimitInMS( 50.0f )
, m_soundChannelsPlayingLimit( 64 )
, m_soundMemLimitInMB( 128 )
, m_numDrawCallsLimit( 2000 )
,	m_width( 0 )
, m_height( 0 )
{
	IConsole* pConsole( gEnv->pConsole );
	if( 0 != pConsole )
	{
		pConsole->Register( c_sys_budget_sysmem, &m_sysMemLimitInMB, m_sysMemLimitInMB, VF_DUMPTODISK, "Sets the upper limit for system memory (in MB) when monitoring budget." );
		pConsole->Register( c_sys_budget_videomem, &m_videoMemLimitInMB, m_videoMemLimitInMB, VF_DUMPTODISK, "Sets the upper limit for video memory (in MB) when monitoring budget." );
		pConsole->Register( c_sys_budget_frametime, &m_frameTimeLimitInMS, m_frameTimeLimitInMS, VF_DUMPTODISK, "Sets the upper limit for frame time (in ms) when monitoring budget." );
		pConsole->Register( c_sys_budget_soundchannels, &m_soundChannelsPlayingLimit, m_soundChannelsPlayingLimit, VF_DUMPTODISK, "Sets the upper limit for sound channels playing when monitoring budget." );
		pConsole->Register( c_sys_budget_soundmem, &m_soundMemLimitInMB, m_soundMemLimitInMB, VF_DUMPTODISK, "Sets the upper limit for sound memory (in MB) when monitoring budget." );
		pConsole->Register( c_sys_budget_numdrawcalls, &m_numDrawCallsLimit, m_numDrawCallsLimit, VF_DUMPTODISK, "Sets the upper limit for number of draw calls per frame." );
	}	
}


CBudgetingSystem::~CBudgetingSystem()
{
}


void 
CBudgetingSystem::Release()
{
	IConsole* pConsole( gEnv->pConsole );
	if( 0 != pConsole )
	{
		pConsole->UnregisterVariable( c_sys_budget_sysmem );
		pConsole->UnregisterVariable( c_sys_budget_videomem );
		pConsole->UnregisterVariable( c_sys_budget_frametime );
		pConsole->UnregisterVariable( c_sys_budget_soundchannels );
		pConsole->UnregisterVariable( c_sys_budget_soundmem );
		pConsole->UnregisterVariable( c_sys_budget_numdrawcalls );
	}

	delete this;
}


void 
CBudgetingSystem::SetSysMemLimit( int sysMemLimitInMB )
{
	m_sysMemLimitInMB = sysMemLimitInMB;
}


void 
CBudgetingSystem::SetVideoMemLimit( int videoMemLimitInMB )
{
	m_videoMemLimitInMB = videoMemLimitInMB;
}


void 
CBudgetingSystem::SetFrameTimeLimit( float frameTimeLimitInMS )
{
	m_frameTimeLimitInMS = frameTimeLimitInMS;
}

void 
CBudgetingSystem::SetSoundChannelsPlayingLimit( int soundChannelsPlayingLimit )
{
	m_soundChannelsPlayingLimit = soundChannelsPlayingLimit;
}

void 
CBudgetingSystem::SetSoundMemLimit( int SoundMemLimit )
{
	m_soundMemLimitInMB = SoundMemLimit;
}

void 
CBudgetingSystem::SetNumDrawCallsLimit( int numDrawCallsLimit )
{
	m_numDrawCallsLimit = numDrawCallsLimit;
}


void 
CBudgetingSystem::SetBudget( int sysMemLimitInMB, int videoMemLimitInMB, 
	float frameTimeLimitInMS, int soundChannelsPlayingLimit, int SoundMemLimitInMB, int numDrawCallsLimit )
{
	m_sysMemLimitInMB = sysMemLimitInMB;
	m_videoMemLimitInMB = videoMemLimitInMB;
	m_frameTimeLimitInMS = frameTimeLimitInMS;
	m_soundChannelsPlayingLimit = soundChannelsPlayingLimit;
	m_soundMemLimitInMB = SoundMemLimitInMB;
	m_numDrawCallsLimit = numDrawCallsLimit;
}


int 
CBudgetingSystem::GetSysMemLimit() const
{
	return( m_sysMemLimitInMB );
}


int 
CBudgetingSystem::GetVideoMemLimit() const
{
	return( m_videoMemLimitInMB );
}


float
CBudgetingSystem::GetFrameTimeLimit() const
{
	return( m_frameTimeLimitInMS );
}


int
CBudgetingSystem::GetSoundChannelsPlayingLimit() const
{
	return( m_soundChannelsPlayingLimit );
}


int
CBudgetingSystem::GetSoundMemLimit() const
{
	return( m_soundMemLimitInMB );
}


int
CBudgetingSystem::GetNumDrawCallsLimit() const
{
	return( m_numDrawCallsLimit );
}


void 
CBudgetingSystem::GetBudget( int& sysMemLimitInMB, int& videoMemLimitInMB, 
	float& frameTimeLimitInMS, int& soundChannelsPlayingLimit, int& soundMemLimitInMB, int& numDrawCallsLimit ) const
{
	sysMemLimitInMB = m_sysMemLimitInMB;
	videoMemLimitInMB = m_videoMemLimitInMB;
	frameTimeLimitInMS = m_frameTimeLimitInMS;
	soundChannelsPlayingLimit = m_soundChannelsPlayingLimit;
	soundMemLimitInMB = m_soundMemLimitInMB;
	numDrawCallsLimit = m_numDrawCallsLimit;
}


void 
CBudgetingSystem::MonitorBudget()
{
	// get required interfaces
	if( 0 == m_pRenderer )
	{
		m_pRenderer = gEnv->pRenderer;
		if( 0 == m_pRenderer )
		{
			return;
		}
	}	

	if( 0 == m_pAuxRenderer )
	{
		m_pAuxRenderer = m_pRenderer->GetIRenderAuxGeom();
		if( 0 == m_pAuxRenderer )
		{
			return;
		}
	}

	if( 0 == m_pTimer )
	{
		m_pTimer = gEnv->pTimer;
		if( 0 == m_pTimer )
		{
			return;
		}
	}	

	if( 0 == m_pISoundSystem )
	{
		m_pISoundSystem = gEnv->pSoundSystem;
		if( 0 == m_pISoundSystem )
		{
			return;
		}
	}	


	// set rendering flags for meters
	SAuxGeomRenderFlags flags( e_Def2DPublicRenderflags );
	flags.SetDepthTestFlag( e_DepthTestOff );
	flags.SetDepthWriteFlag( e_DepthWriteOff );
	flags.SetCullMode( e_CullModeNone );
	m_pAuxRenderer->SetRenderFlags( flags );

	// get height and width of view port
	m_width = m_pRenderer->GetWidth();
	m_height = m_pRenderer->GetHeight();

	// set to 2D mode for font rendering
	m_pRenderer->Set2DMode( true, m_width, m_height );

	// draw meters
	float x( 10 ); float y( 10 );	
	MonitorSystemMemory( x, y );
	MonitorVideoMemory( x, y );
	MonitorFrameTime( x, y );
	MonitorSoundChannels( x, y );
	MonitorSoundMemory( x, y );
	MonitorDrawCalls( x, y );

	// warning if in editor
	if(GetISystem()->IsEditor())
	{
		float color[4] = { 1,0,0,1 };
		DrawText( x, y, color, "WARNING: Editor makes budgets invalid (check it only in pure game)");
	}

	// set back to 3D mode
	m_pRenderer->Set2DMode( false, 0, 0 );
}


void 
CBudgetingSystem::DrawText( float& x, float& y, float* pColor, const char * format, ... )
{	
	char buffer[ 512 ];
	va_list args;
	va_start( args, format );
	vsprintf_s( buffer, sizeof(buffer), format, args );
	va_end( args );

	m_pRenderer->Draw2dLabel( x, y, c_fontScale, pColor, false, "%s", buffer );
	y += c_yStepSizeText;
}


void 
CBudgetingSystem::DrawMeter( float& x, float& y, float scale )
{	
	// draw frame for meter
	uint16 indLines[ 8 ] = 
	{
		0, 1, 1, 2,
		2, 3, 3, 0
	};
	
	Vec3 frame[ 4 ] =
	{
		Vec3( ( x - 1 ) / (float) m_width, ( y - 1 ) / (float) m_height, 0 ),
		Vec3( x / (float) m_width + 0.5f, ( y - 1 ) / (float) m_height, 0 ),
		Vec3( x / (float) m_width + 0.5f, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 ),
		Vec3( ( x - 1 ) / (float) m_width, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 )
	};

	m_pAuxRenderer->DrawLines( frame, 4, indLines, 8, ColorB( 255, 255, 255, 255 ) );

	// draw meter itself
	uint16 indTri[ 6 ] = 
	{
		0, 1, 2, 
		0, 2, 3	
	};	

	// green part (0.0 <= scale <= 0.5)
	{
		float lScale( max( min( scale, 0.5f ), 0.0f ) );
		
		Vec3 bar[ 4 ] =
		{
			Vec3( x / (float) m_width, y / (float) m_height, 0 ),
			Vec3( x / (float) m_width + lScale * 0.5f, y / (float) m_height, 0 ),
			Vec3( x / (float) m_width + lScale * 0.5f, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 ),
			Vec3( x / (float) m_width, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 )
		};
		m_pAuxRenderer->DrawTriangles( bar, 4, indTri, 6, ColorB( 0, 255, 0, 255 ) );
	}

	// green to yellow part (0.5 < scale <= 0.75)
	if( scale > 0.5f )
	{
		float lScale( min( scale, 0.75f ) );
		
		Vec3 bar[ 4 ] =
		{
			Vec3( x / (float) m_width + 0.25f, y / (float) m_height, 0 ),
			Vec3( x / (float) m_width + lScale * 0.5f, y / (float) m_height, 0 ),
			Vec3( x / (float) m_width + lScale * 0.5f, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 ),
			Vec3( x / (float) m_width + 0.25f, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 )
		};
		
		float color[ 4 ];
		GetColor( lScale, color );
		
		ColorB colSegStart( 0, 255, 0, 255 );
		ColorB colSegEnd( (uint8) ( color[ 0 ] * 255 ), (uint8) ( color[ 1 ] * 255 ), (uint8) ( color[ 2 ] * 255 ), (uint8) ( color[ 3 ] * 255 ) );

		ColorB col[ 4 ] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		m_pAuxRenderer->DrawTriangles( bar, 4, indTri, 6, col );
	}

	// yellow to red part (0.75 < scale <= 1.0)
	if( scale > 0.75f  )
	{
		float lScale( min( scale, 1.0f ) );
		
		Vec3 bar[ 4 ] =
		{
			Vec3( x / (float) m_width + 0.375f, y / (float) m_height, 0 ),
			Vec3( x / (float) m_width + lScale * 0.5f, y / (float) m_height, 0 ),
			Vec3( x / (float) m_width + lScale * 0.5f, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 ),
			Vec3( x / (float) m_width + 0.375f, ( y + c_yStepSizeTextMeter ) / (float) m_height, 0 )
		};
		
		float color[ 4 ];
		GetColor( lScale, color );

		ColorB colSegStart( 255, 255, 0, 255 );
		ColorB colSegEnd( (uint8) ( color[ 0 ] * 255 ), (uint8) ( color[ 1 ] * 255 ), (uint8) ( color[ 2 ] * 255 ), (uint8) ( color[ 3 ] * 255 ) );

		ColorB col[ 4 ] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		m_pAuxRenderer->DrawTriangles( bar, 4, indTri, 6, col );
	}

	y += c_yStepSizeTextMeter;
}


void 
CBudgetingSystem::GetColor( float scale, float* pColor )
{	
	if( scale <= 0.5f )
	{
		pColor[ 0 ] = 0;
		pColor[ 1 ] = 1;
		pColor[ 2 ] = 0;
		pColor[ 3 ] = 1;
	}
	else if( scale <= 0.75f )
	{
		pColor[ 0 ] = ( scale - 0.5f ) * 4.0f;
		pColor[ 1 ] = 1;
		pColor[ 2 ] = 0;
		pColor[ 3 ] = 1;
	}
	else if( scale <= 1.0f )
	{
		pColor[ 0 ] = 1;
		pColor[ 1 ] = 1 - ( scale - 0.75f ) * 4.0f;
		pColor[ 2 ] = 0;
		pColor[ 3 ] = 1;
	}
	else
	{
		float time( m_pTimer->GetAsyncCurTime() );
		float blink( sinf( time * 6.28f ) * 0.5f + 0.5f );
		pColor[ 0 ] = 1;
		pColor[ 1 ] = blink;
		pColor[ 2 ] = blink;
		pColor[ 3 ] = 1;
	}
}


void 
CBudgetingSystem::MonitorSystemMemory( float& x, float& y )
{
	int memUsageInMB_Engine( CryMemoryGetAllocatedSize() );
	int memUsageInMB_SysCopyMeshes( *( (int*) m_pRenderer->EF_Query( EFQ_Alloc_APIMesh ) ) );
	int memUsageInMB_SysCopyTextures( *( (int*) m_pRenderer->EF_Query( EFQ_Alloc_APITextures ) ) );

	int memUsageInMB( RoundToClosestMB( (size_t) memUsageInMB_Engine + 
		(size_t) memUsageInMB_SysCopyMeshes + (size_t) memUsageInMB_SysCopyTextures ) );

	memUsageInMB_Engine = RoundToClosestMB( memUsageInMB_Engine );
	memUsageInMB_SysCopyMeshes = RoundToClosestMB( memUsageInMB_SysCopyMeshes );
	memUsageInMB_SysCopyTextures = RoundToClosestMB( memUsageInMB_SysCopyTextures );

	float scale( (float) memUsageInMB / (float) m_sysMemLimitInMB );

	float color[ 4 ];
	GetColor( scale, color );

	if( scale <= 1.0f )
	{
		DrawText( x, y, color, "System memory usage: %d MB (current budget is %d MB).", memUsageInMB, m_sysMemLimitInMB );
	}
	else
	{
		DrawText( x, y, color, "System memory usage: %d MB (current budget is %d MB). You're over budget!!!", memUsageInMB, m_sysMemLimitInMB );
	}

	DrawText( x, y, color, "[%d MB (engine), %d MB (managed textures), %d MB (managed meshes)]", memUsageInMB_Engine, memUsageInMB_SysCopyTextures, memUsageInMB_SysCopyMeshes );
	
	DrawMeter( x, y, scale );
}


void 
CBudgetingSystem::MonitorVideoMemory( float& x, float& y )
{	
	size_t vidMemUsedThisFrame( 0 ), vidMemUsedRecently( 0 );
	m_pRenderer->GetVideoMemoryUsageStats( vidMemUsedThisFrame, vidMemUsedRecently );
	
	int vidMemUsedThisFrameMB = RoundToClosestMB( vidMemUsedThisFrame );
	int vidMemUsedRecentlyMB = RoundToClosestMB( vidMemUsedRecently );

	float scale( (float) vidMemUsedRecentlyMB / (float) m_videoMemLimitInMB );

	float color[ 4 ];
	GetColor( scale, color );

	if( scale <= 1.0f )
		DrawText( x, y, color, "Video mem usage: %d MB this frame / %d MB recently (current budget is: %d MB).", vidMemUsedThisFrameMB, vidMemUsedRecentlyMB, m_videoMemLimitInMB );
	else
		DrawText( x, y, color, "Video mem usage: %d MB this frame / %d MB recently (current budget is: %d MB). You're over budget, texture thrashing is likely to occur!!!", vidMemUsedThisFrameMB, vidMemUsedRecentlyMB, m_videoMemLimitInMB );

	DrawMeter( x, y, scale );
}


void 
CBudgetingSystem::MonitorFrameTime( float& x, float& y )
{
	static float s_fps( 100.0f );
	static float s_startTime( m_pTimer->GetAsyncCurTime() );
	static float s_fpsAccu( 0.0f );
	static int s_numFramesMeasured( 0 );
	
	// accumulate all fps over a period of one second
	s_fpsAccu += m_pTimer->GetFrameRate();
	++s_numFramesMeasured;

	// check if accumulation period ellapsed,
	// if so calc new average fps and reset accumulators
	float curTime( m_pTimer->GetAsyncCurTime() );
	if( curTime - s_startTime >= 1.0f )
	{
		s_fps = s_fpsAccu / s_numFramesMeasured;

		s_startTime = curTime;
		s_fpsAccu = 0.0f;
		s_numFramesMeasured = 0;
	}

	// calc scale and update budget info
	float curFrameTimeInMS( 1000.0f / s_fps );
	float scale( curFrameTimeInMS / m_frameTimeLimitInMS );

	float color[ 4 ];
	GetColor( scale, color );

	if( scale <=  1.0f )
	{
		DrawText( x, y, color, "Frame time: %3.1f ms = %3.1f fps (current budget is %3.1f ms = %3.1f fps).", curFrameTimeInMS, s_fps, m_frameTimeLimitInMS, 1000.0f / m_frameTimeLimitInMS );
	}
	else
	{
		DrawText( x, y, color, "Frame time: %3.1f ms = %3.1f fps (current budget is %3.1f ms = %3.1f fps). You're over budget!!!", curFrameTimeInMS, s_fps, m_frameTimeLimitInMS, 1000.0f / m_frameTimeLimitInMS );
	}

	DrawMeter( x, y, scale );
}


void 
CBudgetingSystem::MonitorSoundChannels( float& x, float& y )
{	
	int channelsPlaying(  m_pISoundSystem->GetUsedVoices() );
	float scale( (float) channelsPlaying / (float) m_soundChannelsPlayingLimit );

	float color[ 4 ];
	GetColor( scale, color );

	if( scale <= 1.0f )
	{
		DrawText( x, y, color, "Sound Channels playing: %d (current budget is %d).", channelsPlaying, m_soundChannelsPlayingLimit );
	}
	else
	{
		DrawText( x, y, color, "Sound Channels playing: %d (current budget is %d). You're over budget!!!", channelsPlaying, m_soundChannelsPlayingLimit );
	}

	DrawMeter( x, y, scale );
}

void 
CBudgetingSystem::MonitorSoundMemory( float& x, float& y )
{	
	int nSizeInMB(  m_pISoundSystem->GetMemoryUsageInMB() );
	float scale( (float) nSizeInMB / (float) m_soundMemLimitInMB );

	float color[ 4 ];
	GetColor( scale, color );

	if( scale <= 1.0f )
	{
			DrawText( x, y, color, "Sound mem usage: %d MB (current budget is %d).", nSizeInMB, m_soundMemLimitInMB );
	}
	else
	{
		DrawText( x, y, color, "Sound mem usage: %d MB (current budget is %d). You're over budget!!!", nSizeInMB, m_soundMemLimitInMB );
	}

	DrawMeter( x, y, scale );
}


void 
CBudgetingSystem::MonitorDrawCalls( float& x, float& y )
{	
	int numDrawCalls( m_pRenderer->GetCurrentNumberOfDrawCalls() );
	float scale( (float) numDrawCalls / (float) m_numDrawCallsLimit );

	float color[ 4 ];
	GetColor( scale, color );

	if( scale <= 1.0f )
	{
		DrawText( x, y, color, "Number of draw calls: %d (current budget is %d).", numDrawCalls, m_numDrawCallsLimit );
	}
	else
	{
		DrawText( x, y, color, "Number of draw calls: %d (current budget is %d). You're over budget!!!", numDrawCalls, m_numDrawCallsLimit );
	}

	DrawMeter( x, y, scale );
}
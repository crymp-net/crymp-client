#include "ActorIterator.h"
#include "ActorSystem.h"

ActorIterator::ActorIterator(ActorSystem* pActorSystem) : m_pActorSystem(pActorSystem)
{
	this->Init();
}

void ActorIterator::Init()
{
	// TODO: this might get invalidated by std::map::erase -> unsafe
	m_iterator = m_pActorSystem->m_actors.begin();
	m_refCount = 0;
}

////////////////////////////////////////////////////////////////////////////////
// IActorIterator
////////////////////////////////////////////////////////////////////////////////

size_t ActorIterator::Count()
{
	return m_pActorSystem->m_actors.size();
}

IActor* ActorIterator::Next()
{
	if (m_iterator == m_pActorSystem->m_actors.end())
	{
		return nullptr;
	}

	IActor* pActor = m_pActorSystem->DecodeActor(m_iterator->second);
	++m_iterator;

	return pActor;
}

void ActorIterator::AddRef()
{
	++m_refCount;
}

void ActorIterator::Release()
{
	if (--m_refCount <= 0)
	{
		m_pActorSystem->m_iteratorPool.emplace_back(this);
	}
}

////////////////////////////////////////////////////////////////////////////////

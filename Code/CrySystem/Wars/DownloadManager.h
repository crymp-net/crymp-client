#pragma once

#ifdef WIN32

class CHTTPDownloader;


class CDownloadManager
{
public:
	CDownloadManager();
	virtual ~CDownloadManager();

	void Create(ISystem *pSystem);
	CHTTPDownloader *CreateDownload();
	void RemoveDownload(CHTTPDownloader *pDownload);
	void Update();
	void Release();

private:

	ISystem												*m_pSystem;
	std::list<CHTTPDownloader *>	m_lDownloadList;
};


#endif //LINUX
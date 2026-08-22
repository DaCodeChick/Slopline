/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */


#pragma mark ##Constants##
#pragma mark Transaction IDs
enum {
	myTran_Error				= 100,  
	myTran_GetMsgs				= 101,
	myTran_NewMsg				= 102,
	myTran_PostMsg				= 103,
	myTran_ServerMsg			= 104,
	myTran_ChatSend				= 105,
	myTran_ChatMsg				= 106,
	myTran_Login				= 107,
	myTran_SendInstantMsg		= 108,
	myTran_ShowAgreement		= 109,
	myTran_DisconnectUser		= 110,
	myTran_DisconnectMsg		= 111,
	myTran_InviteNewChat		= 112,
	myTran_InviteToChat			= 113,
	myTran_RejectChatInvite		= 114,
	myTran_JoinChat				= 115,
	myTran_LeaveChat			= 116,
	myTran_NotifyChatChangeUser	= 117,
	myTran_NotifyChatDeleteUser	= 118,
	myTran_NotifyChatSubject	= 119,
	myTran_SetChatSubject		= 120,
	myTran_Agreed				= 121,
	myTran_ServerBanner			= 122,
	myTran_IconChange			= 123,
	
	myTran_NickChange			= 124,
	myTran_FakeRed				= 125,
	myTran_Away					= 126,
	myTran_CrazyServer			= 127,
	myTran_BlockDownload		= 128,
	myTran_Visible				= 129,
	myTran_AdminSpector			= 130,
	myTran_StandardMessage		= 131,

	myTran_GetFileNameList		= 200,
	myTran_DownloadFile			= 202,
	myTran_UploadFile			= 203,
	myTran_DeleteFile			= 204,
	myTran_NewFolder			= 205,
	myTran_GetFileInfo			= 206,
	myTran_SetFileInfo			= 207,
	myTran_MoveFile				= 208,
	myTran_MakeFileAlias		= 209,
	myTran_DownloadFldr			= 210,
	myTran_DownloadInfo			= 211,
	myTran_DownloadBanner		= 212,
	myTran_UploadFldr			= 213,
	myTran_KillDownload			= 214,
	
	myTran_GetUserNameList		= 300,
	myTran_NotifyChangeUser		= 301,
	myTran_NotifyDeleteUser		= 302,
	myTran_GetClientInfoText	= 303,
	myTran_SetClientUserInfo	= 304,
	
	myTran_GetUserList			= 348,
	myTran_SetUserList			= 349,
	myTran_NewUser				= 350,
	myTran_DeleteUser			= 351,
	myTran_GetUser				= 352,
	myTran_SetUser				= 353,
	
	myTran_UserAccess			= 354,
	myTran_UserBroadcast		= 355,
	
	myTran_GetNewsCatNameList	= 370,
	myTran_GetNewsArtNameList	= 371,
	myTran_DelNewsItem			= 380,
	myTran_NewNewsFldr			= 381,
	myTran_NewNewsCat			= 382,
	myTran_GetNewsArtData		= 400,
	myTran_PostNewsArt			= 410,
	myTran_DelNewsArt			= 411,
	
	myTran_KeepConnectionAlive	= 500	
};

#pragma mark Field IDs
enum {
//hope
	myField_SessionKey			= 3587, // ((u_int16_t) 0x0e03)
	myField_MacAlg				= 3588, // ((u_int16_t) 0x0e04)
	
	//crypt
	myField_S_CipherAlg			= 0x0ec1, // 3771
	myField_C_CipherAlg			= 0x0ec2, // 3772
	
	myField_ErrorText			= 100,
	myField_Data				= 101,
	myField_UserName			= 102,
	myField_UserID				= 103,
	myField_UserIconID			= 104,
	myField_UserLogin			= 105,
	myField_UserPassword		= 106,
	myField_RefNum				= 107,
	myField_TransferSize		= 108,
	myField_ChatOptions			= 109,
	myField_UserAccess			= 110,
	myField_UserAlias			= 111,
	myField_UserFlags			= 112,
	myField_Options				= 113,
	myField_ChatID				= 114,
	myField_ChatSubject			= 115,
	myField_WaitingCount		= 116,
	myField_IconId				= 117,
	myField_NickName			= 118,
	
	myField_FakeRed				= 119,
	myField_Away				= 120,
	myField_BlockDownload		= 121,
	myField_Visible				= 112,
	myField_AdminSpector		= 122,
	myField_StandardMessage		= 123,
	
	myField_number				= 114,
	myField_ServerAgreement		= 150,
	myField_ServerBanner		= 151,
	myField_ServerBannerType	= 152,
	myField_ServerBannerUrl		= 153,
	myField_NoServerAgreement	= 154,
	myField_Vers				= 160,
	myField_CommunityBannerID	= 161,
	myField_ServerName			= 162,

	myField_FileNameWithInfo	= 200,
	myField_FileName			= 201,
	myField_FilePath			= 202,
	myField_FileResumeData		= 203,
	myField_FileXferOptions		= 204,
	myField_FileTypeString		= 205,
	myField_FileCreatorString	= 206,
	myField_FileSize			= 207,
	myField_FileCreateDate		= 208,
	myField_FileModifyDate		= 209,
	myField_FileComment			= 210,
	myField_FileNewName			= 211,
	myField_FileNewPath			= 212,
	myField_FileType			= 213,

	myField_QuotingMsg			= 214,
	myField_AutomaticResponse	= 215,
	myField_FldrItemCount		= 220,

	myField_UserNameWithInfo	= 300,

	myField_NewsCatGUID			= 319,
	myField_NewsCatListData		= 320,	// for 1.5 client/servers prior to April 15
	myField_NewsArtListData		= 321,
	myField_NewsCatName			= 322,
	myField_NewsCatListData15	= 323,
	myField_NewsPath			= 325,
	myField_NewsArtID			= 326,
	
	myField_NewsArtDataFlav		= 327,
	myField_NewsArtTitle		= 328,
	myField_NewsArtPoster		= 329,
	myField_NewsArtDate			= 330,
	myField_NewsArtPrevArt		= 331,
	myField_NewsArtNextArt		= 332,
	myField_NewsArtData			= 333,
	myField_NewsArtFlags		= 334,
	myField_NewsArtParentArt	= 335,
	myField_NewsArt1stChildArt	= 336,
	myField_NewsArtRecurseDel	= 337

};

#pragma mark Access Privs
enum {
	myAcc_DeleteFile		= 0,
	myAcc_UploadFile		= 1,
	myAcc_DownloadFile		= 2,
	myAcc_RenameFile		= 3,
	myAcc_MoveFile			= 4,
	myAcc_CreateFolder		= 5,
	myAcc_DeleteFolder		= 6,
	myAcc_RenameFolder		= 7,
	myAcc_MoveFolder		= 8,
	myAcc_ReadChat			= 9,
	myAcc_SendChat			= 10,
	myAcc_CreateChat		= 11,
	myAcc_CloseChat			= 12,
	myAcc_ShowInList		= 13,
	myAcc_CreateUser		= 14,
	myAcc_DeleteUser		= 15,
	myAcc_OpenUser			= 16,
	myAcc_ModifyUser		= 17,
	myAcc_ChangeOwnPass		= 18,
	myAcc_SendPrivMsg		= 19,
	myAcc_NewsReadArt		= 20,
	myAcc_NewsPostArt		= 21,
	myAcc_DisconUser		= 22,
	myAcc_CannotBeDiscon	= 23,
	myAcc_GetClientInfo		= 24,
	myAcc_UploadAnywhere	= 25,
	myAcc_AnyName			= 26,
	myAcc_NoAgreement		= 27,
	myAcc_SetFileComment	= 28,
	myAcc_SetFolderComment	= 29,
	myAcc_ViewDropBoxes		= 30,
	myAcc_MakeAlias			= 31,
	myAcc_Broadcast			= 32,
	myAcc_NewsDeleteArt		= 33,
	myAcc_NewsCreateCat		= 34,
	myAcc_NewsDeleteCat		= 35,
	myAcc_NewsCreateFldr	= 36,
	myAcc_NewsDeleteFldr	= 37,
	myAcc_UploadFolder		= 38,
	myAcc_DownloadFolder	= 39,
	myAcc_SendMessage		= 40,
	myAcc_FakeRed			= 41,
	myAcc_Away				= 42,
	myAcc_ChangeNick		= 43,
	myAcc_ChangeIcon		= 44,
	myAcc_SpeakBefore		= 45,
	myAcc_RefuseChat		= 46,
	myAcc_BlockDownload		= 47,
	myAcc_Visible			= 48,
	myAcc_Canviewinvisible	= 49,
	myAcc_Canflood			= 50,
	myAcc_ViewOwnDropBox	= 51,
	myAcc_DontQueue			= 52,
	myAcc_AdmInSpector		= 53,
	myAcc_PostBefore		= 54
	
	
};

enum
{
	myOpt_UserMessage		= 1,
	myOpt_RefuseMessage		= 2,
	myOpt_RefuseChat		= 3,
	myOpt_AutomaticResponse	= 4
};

enum
{
	dlFldrAction_SendFile		= 1,
	dlFldrAction_ResumeFile		= 2,
	dlFldrAction_NextFile		= 3
};


#pragma mark -

#pragma mark ##Structs##
// SMyFileInfo
#pragma mark SMyFileInfo
#pragma options align=packed
struct SMyFileInfo {
	Uint32 type, creator;
	Uint32 fileSize;
	Uint32 rsvd;
	Uint16 nameScript;
	Uint16 nameSize;
	Uint8 nameData[];
};
#pragma options align=reset

// SMyUserAccess
#pragma mark SMyUserAccess
#pragma options align=packed
struct SMyUserAccess {
	Uint32 data[2];
	
	void Clear()												{	data[0] = data[1] = 0;															}
	void Fill()													{	data[0] = data[1] = 0xFFFFFFFF;													}
	
	void SetPriv(Uint32 inPriv, bool inAllow = true)			{	SetBit(data, inPriv, inAllow);													}
	bool HasPriv(Uint32 inPriv)	const							{	return GetBit(data, inPriv);													}
	bool HasNoPriv() const										{	return (data[0] == 0 && data[1] == 0);											}
	bool HasAllPriv() const										{	return (HasPriv(myAcc_DownloadFile) && HasPriv(myAcc_DownloadFolder) && HasPriv(myAcc_SpeakBefore) && HasPriv(myAcc_PostBefore) &&  HasPriv(myAcc_UploadFile) && HasPriv(myAcc_UploadFolder) && 
																			HasPriv(myAcc_UploadAnywhere) && HasPriv(myAcc_DeleteFile) && HasPriv(myAcc_RenameFile) && HasPriv(myAcc_MoveFile) && 
																			HasPriv(myAcc_SetFileComment) && HasPriv(myAcc_CreateFolder) && HasPriv(myAcc_DeleteFolder) && HasPriv(myAcc_RenameFolder) && 
																			HasPriv(myAcc_MoveFolder) && HasPriv(myAcc_SetFolderComment) && HasPriv(myAcc_ViewDropBoxes) && HasPriv(myAcc_MakeAlias) && 
																			HasPriv(myAcc_CreateUser) && HasPriv(myAcc_DeleteUser) && HasPriv(myAcc_OpenUser) && HasPriv(myAcc_ModifyUser) && 
																			HasPriv(myAcc_GetClientInfo) && HasPriv(myAcc_AdmInSpector) && HasPriv(myAcc_DisconUser) && HasPriv(myAcc_CannotBeDiscon) && HasPriv(myAcc_SendMessage) &&
																			HasPriv(myAcc_Broadcast) && HasPriv(myAcc_NewsReadArt) && HasPriv(myAcc_NewsPostArt) && HasPriv(myAcc_NewsDeleteArt) && 
																			HasPriv(myAcc_NewsCreateCat) && HasPriv(myAcc_NewsDeleteCat) && HasPriv(myAcc_NewsCreateFldr) && HasPriv(myAcc_FakeRed) && HasPriv(myAcc_RefuseChat) && HasPriv(myAcc_Canviewinvisible) && HasPriv(myAcc_Canflood) && HasPriv(myAcc_Visible) && HasPriv(myAcc_BlockDownload) && HasPriv(myAcc_Away) && HasPriv(myAcc_ChangeNick)  && HasPriv(myAcc_ChangeIcon) && HasPriv(myAcc_NewsDeleteFldr) && 
																			HasPriv(myAcc_CreateChat) && HasPriv(myAcc_ViewOwnDropBox) && HasPriv(myAcc_DontQueue) && HasPriv(myAcc_ReadChat) && HasPriv(myAcc_SendChat) && HasPriv(myAcc_AnyName));
																			/* ignore myAcc_NoAgreement */											}
	void SetAllPriv()											{	Clear();SetPriv(myAcc_DownloadFile); SetPriv(myAcc_DownloadFolder); SetPriv(myAcc_UploadFile); SetPriv(myAcc_SpeakBefore); SetPriv(myAcc_PostBefore); SetPriv(myAcc_UploadFolder);
																			SetPriv(myAcc_UploadAnywhere); SetPriv(myAcc_DeleteFile); SetPriv(myAcc_RenameFile); SetPriv(myAcc_MoveFile);
																			SetPriv(myAcc_SetFileComment); SetPriv(myAcc_CreateFolder); SetPriv(myAcc_DeleteFolder); SetPriv(myAcc_RenameFolder);
																			SetPriv(myAcc_MoveFolder); SetPriv(myAcc_SetFolderComment); SetPriv(myAcc_ViewDropBoxes); SetPriv(myAcc_MakeAlias);
																			SetPriv(myAcc_CreateUser); SetPriv(myAcc_DeleteUser); SetPriv(myAcc_OpenUser); SetPriv(myAcc_ModifyUser);
																			SetPriv(myAcc_GetClientInfo); SetPriv(myAcc_AdmInSpector); SetPriv(myAcc_DisconUser); SetPriv(myAcc_CannotBeDiscon); SetPriv(myAcc_SendMessage);
																			SetPriv(myAcc_Broadcast); SetPriv(myAcc_NewsReadArt); SetPriv(myAcc_NewsPostArt); SetPriv(myAcc_NewsDeleteArt);
																			SetPriv(myAcc_NewsCreateCat); SetPriv(myAcc_NewsDeleteCat); SetPriv(myAcc_NewsCreateFldr); SetPriv(myAcc_FakeRed); SetPriv(myAcc_RefuseChat); SetPriv(myAcc_Canviewinvisible); SetPriv(myAcc_Canflood); SetPriv(myAcc_Visible); SetPriv(myAcc_BlockDownload); SetPriv(myAcc_Away); SetPriv(myAcc_ChangeNick); SetPriv(myAcc_ChangeIcon); SetPriv(myAcc_NewsDeleteFldr);
																			SetPriv(myAcc_CreateChat); SetPriv(myAcc_DontQueue); SetPriv(myAcc_ViewOwnDropBox); SetPriv(myAcc_ReadChat); SetPriv(myAcc_SendChat); SetPriv(myAcc_AnyName);
																			/* ignore myAcc_NoAgreement */											}

	bool operator == (const SMyUserAccess& inUserAccess) const	{ 	return (data[0] == inUserAccess.data[0] && data[1] == inUserAccess.data[1]);	}
	bool operator != (const SMyUserAccess& inUserAccess) const	{ 	return (data[0] != inUserAccess.data[0] || data[1] != inUserAccess.data[1]); 	}
};
#pragma options align=reset

// SMyUserDataFile
#pragma mark SMyUserDataFile
enum {
	kMyUserDataFileVersion	= 1
};
#pragma options align=packed
struct SMyUserDataFile {
	Uint16 version;
	Int16 iconID;
	
	SMyUserAccess access;
	Uint16 maxSimulDownloads;
	Uint8 rsvd[512];
	Uint16 nameScript;
	Uint16 nameSize;
	Uint8 nameData[64];
	Uint16 aliasScript;
	Uint16 aliasSize;
	Uint8 aliasData[64];
	Uint16 loginScript;
	Uint16 loginSize;
	Uint8 loginData[32];
	Uint16 passwordScript;
	Uint16 passwordSize;
	Uint8 passwordData[32];
};
#pragma options align=reset


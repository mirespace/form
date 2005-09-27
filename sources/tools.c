/*
  	#[ Includes :
#define MALLOCDEBUG 1
*/


#include "form3.h"
 
FILES **filelist;
int numinfilelist = 0;
int filelistsize = 0;
static LONG maxlistsize = (LONG)(MAXPOSITIVE);

#ifdef MALLOCDEBUG
#define BANNER 16
void *malloclist[6000];
long mallocsizes[6000];
char *mallocstrings[6000];
int nummalloclist = 0;
#endif

#ifdef GPP
extern "C" getdtablesize();
#endif

/*
  	#] Includes :
  	#[ Streams :
 		#[ LoadInputFile :
*/

UBYTE *
LoadInputFile ARG2(UBYTE *,filename,int,type)
{
	int handle;
	long filesize;
	UBYTE *buffer, *name = filename;
	POSITION scrpos;
	handle = LocateFile(&name,type);
	if ( handle < 0 ) return(0);
	PUTZERO(scrpos);
	SeekFile(handle,&scrpos,SEEK_END);
	TELLFILE(handle,&scrpos);
	filesize = BASEPOSITION(scrpos);
	PUTZERO(scrpos);
	SeekFile(handle,&scrpos,SEEK_SET);
	buffer = (UBYTE *)Malloc1(filesize+1,"LoadInputFile");
	if ( ReadFile(handle,buffer,filesize) != filesize ) {
		Error1("Read error for file ",name);
		M_free(buffer,"LoadInputFile");
		CloseFile(handle);
		return(0);
	}
	CloseFile(handle);
	buffer[filesize] = 0;
	return(buffer);
}

/*
 		#] LoadInputFile :
 		#[ ReadFromStream :
*/

UBYTE
ReadFromStream ARG1(STREAM *,stream)
{
	UBYTE c;
	POSITION scrpos;
#ifdef WITHPIPE
	if ( stream->type == PIPESTREAM ) {
		int cc;
		cc = getc((FILE *)(filelist[stream->handle]));
		if ( cc == EOF ) return(ENDOFSTREAM);
		c = cc;
		if ( stream->eqnum == 1 ) { stream->eqnum = 0; stream->linenumber++; }
		if ( c == LINEFEED ) stream->eqnum = 1;
		return(c);
	}
#endif
/*[14apr2004 mt]:*/
#ifdef WITHEXTERNALCHANNEL
	if ( stream->type == EXTERNALCHANNELSTREAM ) {
		int cc;
		cc = getcFromExtChannel();
		if ( cc == EOF ) return(ENDOFSTREAM);
		c = cc;
		if ( stream->eqnum == 1 ) { stream->eqnum = 0; stream->linenumber++; }
		if ( c == LINEFEED ) stream->eqnum = 1;
		return(c);
	}
#endif /*ifdef WITHEXTERNALCHANNEL*/
/*:[14apr2004 mt]*/
	if ( stream->pointer >= stream->top ) {
		if ( stream->type != FILESTREAM ) return(ENDOFSTREAM);
		if ( stream->fileposition != stream->bufferposition+stream->inbuffer ) {
			stream->fileposition = stream->bufferposition+stream->inbuffer;
			SETBASEPOSITION(scrpos,stream->fileposition);
			SeekFile(stream->handle,&scrpos,SEEK_SET);
		}
		stream->bufferposition = stream->fileposition;
		stream->inbuffer = ReadFile(stream->handle,
				stream->buffer,stream->buffersize);
		if ( stream->inbuffer <= 0 ) return(ENDOFSTREAM);
		stream->top = stream->buffer + stream->inbuffer;
		stream->pointer = stream->buffer;
		stream->fileposition = stream->bufferposition + stream->inbuffer;
	}
	if ( stream->eqnum == 1 ) { stream->eqnum = 0; stream->linenumber++; }
	c = *(stream->pointer)++;
	if ( c == LINEFEED ) stream->eqnum = 1;
	return(c);
}

/*
 		#] ReadFromStream :
 		#[ GetFromStream :
*/

UBYTE
GetFromStream ARG1(STREAM *,stream)
{
	UBYTE c1, c2;
	if ( stream->isnextchar > 0 ) {
		return(stream->nextchar[--stream->isnextchar]);
	}
	c1 = ReadFromStream(stream);
	if ( c1 == LINEFEED || c1 == CARRIAGERETURN ) {
		c2 = ReadFromStream(stream);
		if ( c2 == c1 || ( c2 != LINEFEED && c2 != CARRIAGERETURN ) ) {
			stream->isnextchar = 1;
			stream->nextchar[0] = c2;
		}
		return(LINEFEED);
	}
	else return(c1);
}

/*
 		#] GetFromStream :
 		#[ LookInStream :
*/

UBYTE
LookInStream ARG1(STREAM *,stream)
{
	UBYTE c = GetFromStream(stream);
	UngetFromStream(stream,c);
	return(c);
}

/*
 		#] LookInStream :
 		#[ OpenStream :
*/

STREAM *
OpenStream ARG4(UBYTE *,name,int,type,int,prevarmode,int,raiselow)
{
	STREAM *stream;
	UBYTE *rhsofvariable, *s, *newname;
	POSITION scrpos;
	int handle, num;
	long filesize;
	switch ( type ) {
		case FILESTREAM:
/*
			Notice that FILESTREAM is only used for text files:
			The #include files and the main input file (.frm)
			Hence we do not worry about files longer than 2 Gbytes.
*/
			newname = name;
			handle = LocateFile(&newname,-1);
			if ( handle < 0 ) return(0);
			PUTZERO(scrpos);
			SeekFile(handle,&scrpos,SEEK_END);
			TELLFILE(handle,&scrpos);
			filesize = BASEPOSITION(scrpos);
			PUTZERO(scrpos);
			SeekFile(handle,&scrpos,SEEK_SET);
			if ( filesize > AM.MaxStreamSize ) filesize = AM.MaxStreamSize;
			stream = CreateStream((UBYTE *)"filestream");
			stream->buffer = (UBYTE *)Malloc1(filesize,"name of input stream");
			stream->inbuffer = ReadFile(handle,stream->buffer,filesize);
			stream->top = stream->buffer + stream->inbuffer;
			stream->pointer = stream->buffer;
			stream->handle = handle;
			stream->buffersize = filesize;
			stream->fileposition = stream->inbuffer;
			if ( newname != name ) stream->name = newname;
			else if ( name ) stream->name = strDup1(name,"name of input stream");
			else
				stream->name = 0;
			stream->prevline = stream->linenumber = 1;
			stream->eqnum = 0;
			break;
		case PREVARSTREAM:
			if ( ( rhsofvariable = GetPreVar(name,WITHERROR) ) == 0 ) return(0);
			stream = CreateStream((UBYTE *)"var-stream");
			stream->buffer = stream->pointer = s = rhsofvariable;
			while ( *s ) s++;
			stream->top = s;
			stream->inbuffer = s - stream->buffer;
			stream->name = AC.CurrentStream->name;
			stream->linenumber = AC.CurrentStream->linenumber;
			stream->prevline = AC.CurrentStream->prevline;
			stream->eqnum = AC.CurrentStream->eqnum;
			stream->pname = name;
			stream->olddelay = AP.AllowDelay;
			UnsetAllowDelay();
			break;
		case DOLLARSTREAM:
			if ( ( num = GetDollar(name) ) < 0 ) return(0);
			stream = CreateStream((UBYTE *)"dollar-stream");
			stream->buffer = stream->pointer = s = WriteDollarToBuffer(num);
			while ( *s ) s++;
			stream->top = s;
			stream->inbuffer = s - stream->buffer;
			stream->name = AC.CurrentStream->name;
			stream->linenumber = AC.CurrentStream->linenumber;
			stream->prevline= AC.CurrentStream->prevline;
			stream->eqnum = AC.CurrentStream->eqnum;
			stream->pname = name;
			/* We 'stole' the buffer. Later we can free it. */
			AO.DollarOutSizeBuffer = 0;
			AO.DollarOutBuffer = 0;
			AO.DollarInOutBuffer = 0;
			break;
		case PREREADSTREAM:
		case PREREADSTREAM2:
		case PRECALCSTREAM:
			stream = CreateStream((UBYTE *)"calculator");
			stream->buffer = stream->pointer = s = name;
			while ( *s ) s++;
			stream->top = s;
			stream->inbuffer = s - stream->buffer;
			stream->name = AC.CurrentStream->name;
			stream->linenumber = AC.CurrentStream->linenumber;
			stream->prevline = AC.CurrentStream->prevline;
			stream->eqnum = 0;
			break;
#ifdef WITHPIPE
		case PIPESTREAM:
			stream = CreateStream((UBYTE *)"pipe");
			{
				FILE *f;
				if ( ( f = popen((char *)name,"r") ) == 0 ) {
					Error0("@Cannot create pipe");
				}
				stream->handle = CreateHandle();
				filelist[stream->handle] = (FILES *)f;
			}
			stream->buffer = stream->top = 0;
			stream->inbuffer = 0;
			stream->name = strDup1((UBYTE *)"pipe","pipe");
			stream->prevline = stream->linenumber = 1;
			stream->eqnum = 0;
			break;
#endif
/*[14apr2004 mt]:*/
#ifdef WITHEXTERNALCHANNEL
		case EXTERNALCHANNELSTREAM:
			{/*Block*/
			int n;
			if( (n=getCurrentExternalChannel()) == 0 )
				Error0("@No current extrenal channel");
			stream = CreateStream((UBYTE *)"externalchannel");
			stream->handle = CreateHandle();
			*( (int*)(filelist[stream->handle] = 
				Malloc1(sizeof(int),"external channel handle")
			         ) 
			 )=n;
			}/*Block*/
			stream->buffer = stream->top = 0;
			stream->inbuffer = 0;
			stream->name = strDup1((UBYTE *)"externalchannel","externalchannel");
			stream->prevline = stream->linenumber = 1;
			stream->eqnum = 0;
			break;
#endif /*ifdef WITHEXTERNALCHANNEL*/
/*:[14apr2004 mt]*/
		default:
			return(0);
	}
	stream->bufferposition = 0;
	stream->isnextchar = 0;
	stream->type = type;
	stream->previousNoShowInput = AC.NoShowInput;
	stream->afterwards = raiselow;
	if ( AC.CurrentStream ) stream->previous = AC.CurrentStream - AC.Streams;
	else stream->previous = -1;
	stream->FoldName = 0;
	if ( prevarmode == 0 ) stream->prevars = -1;
	else if ( prevarmode > 0 ) stream->prevars = NumPre;
	else if ( prevarmode < 0 ) stream->prevars = -prevarmode-1;
	AC.CurrentStream = stream;
	if ( type == PREREADSTREAM || type == PRECALCSTREAM
		|| type == DOLLARSTREAM ) AC.NoShowInput++;
	return(stream);
}

/*
 		#] OpenStream :
 		#[ LocateFile :
*/

int
LocateFile ARG2(UBYTE **,name,int,type)
{
	int handle, namesize, i;
	UBYTE *s, *to, *u1, *u2, *newname, *indir;	
	handle = OpenFile((char *)(*name));
	if ( handle >= 0 ) return(handle);
	if ( type == SETUPFILE && AM.SetupFile ) {
		handle = OpenFile((char *)(AM.SetupFile));
		if ( handle >= 0 ) return(handle);
		MesPrint("Could not open setup file %s",(char *)(AM.SetupFile));
	}
	namesize = 2; s = *name;
	while ( *s ) { s++; namesize++; }
	if ( type == SETUPFILE ) indir = AM.SetupDir;
	else indir = AM.IncDir;
	if ( indir ) {

		s = indir; i = 0;
		while ( *s ) { s++; i++; }
		newname = (UBYTE *)Malloc1(namesize+i,"LocateFile");
		s = indir; to = newname;
		while ( *s ) *to++ = *s++;
		if ( to > newname && to[-1] != SEPARATOR ) *to++ = SEPARATOR;
		s = *name;
		while ( *s ) *to++ = *s++;
		*to = 0;
		handle = OpenFile((char *)newname);
		if ( handle >= 0 ) {
			*name = newname;
			return(handle);
		}
		M_free(newname,"LocateFile, incdir/file");
	}
	if ( type == SETUPFILE ) {
		handle = OpenFile(setupfilename);
		if ( handle >= 0 ) return(handle);
		s = (UBYTE *)getenv("FORMSETUP");
		if ( s ) {
			handle = OpenFile((char *)s);
			if ( handle >= 0 ) return(handle);
			MesPrint("Could not open setup file %s",s);
		}
	}
	if ( type != SETUPFILE && AM.Path ) {
		u1 = AM.Path;
		while ( *u1 ) {
			u2 = u1; i = 0;
			while ( *u1 && *u1 != ':' ) {
				if ( *u1 == '\\' ) u1++;
				u1++; i++;
			}
			newname = (UBYTE *)Malloc1(namesize+i,"LocateFile");
			s = u2; to = newname;
			while ( s < u1 ) {
				if ( *s == '\\' ) s++;
				*to++ = *s++;
			}
			if ( to > newname && to[-1] != SEPARATOR ) *to++ = SEPARATOR;
			s = *name;
			while ( *s ) *to++ = *s++;
			*to = 0;
			handle = OpenFile((char *)newname);
			if ( handle >= 0 ) {
				*name = newname;
				return(handle);
			}
			M_free(newname,"LocateFile Path/file");
			if ( *u1 ) u1++;
		}
	}
	if ( type != SETUPFILE ) Error1("LocateFile: Cannot find file",*name);
	return(-1);
}

/*
 		#] LocateFile :
 		#[ CloseStream :
*/

STREAM *
CloseStream ARG1(STREAM *,stream)
{
	int newstr = stream->previous, sgn;
	UBYTE *t, numbuf[24];
	LONG x;
	if ( stream->FoldName ) {
		M_free(stream->FoldName,"stream->FoldName");
		stream->FoldName = 0;
	}
	if ( stream->type == FILESTREAM ) CloseFile(stream->handle);
#ifdef WITHPIPE
	else if ( stream->type == PIPESTREAM ) {
		pclose((FILE *)(filelist[stream->handle]));
		filelist[stream->handle] = 0;
		numinfilelist--;
	}
#endif
/*[14apr2004 mt]:*/
#ifdef WITHEXTERNALCHANNEL
	else if ( stream->type == EXTERNALCHANNELSTREAM ) {
      M_free(filelist[stream->handle],"external channel handle");
		filelist[stream->handle] = 0;
		numinfilelist--;
	}
#endif /*ifdef WITHEXTERNALCHANNEL*/
/*:[14apr2004 mt]*/
	else if ( stream->type == PREVARSTREAM && (
	stream->afterwards == PRERAISEAFTER || stream->afterwards == PRELOWERAFTER ) ) {
		t = stream->buffer; x = 0; sgn = 1;
		while ( *t == '-' || *t == '+' ) {
			if ( *t == '-' ) sgn = -sgn;
			t++;
		}
		if ( FG.cTable[*t] == 1 ) {
			while ( *t && FG.cTable[*t] == 1 ) x = 10*x + *t++ - '0';
			if ( *t == 0 ) {
				if ( stream->afterwards == PRERAISEAFTER ) x = sgn*x + 1;
				else x = sgn*x - 1;
				NumToStr(numbuf,x);
				PutPreVar(stream->pname,numbuf,0,1);
			}
		}
	}
	else if ( stream->type == DOLLARSTREAM && (
	stream->afterwards == PRERAISEAFTER || stream->afterwards == PRELOWERAFTER ) ) {
		if ( stream->afterwards == PRERAISEAFTER ) x = 1;
		else x = -1;
		DollarRaiseLow(stream->pname,x);
	}
	else if ( stream->type == PRECALCSTREAM || stream->type == DOLLARSTREAM ) {
		if ( stream->buffer ) M_free(stream->buffer,"stream->buffer");
		stream->buffer = 0;
	}
	if ( stream->name && stream->type != PREVARSTREAM
	&& stream->type != PREREADSTREAM && stream->type != PREREADSTREAM2
	&& stream->type != PRECALCSTREAM && stream->type != DOLLARSTREAM ) {
		M_free(stream->name,"stream->name");
	}
	stream->name = 0;
/*	if ( stream->type != FILESTREAM )  */
		AC.NoShowInput = stream->previousNoShowInput;
	stream->buffer = 0;		/* To make sure we will not reuse it */
	stream->pointer = 0;
/*
	Look whether we have to pop preprocessor variables.
*/
	if ( stream->prevars >= 0 ) {
		while ( NumPre > stream->prevars ) {
			NumPre--;
			M_free(PreVar[NumPre].name,"PreVar[NumPre].name");
			PreVar[NumPre].name = PreVar[NumPre].value = 0;
		}
	}
	if ( stream->type == PREVARSTREAM ) {
		AP.AllowDelay = stream->olddelay;
		ClearMacro(stream->pname);
	}
	AC.NumStreams--;
	if ( newstr >= 0 ) return(AC.Streams + newstr);
	else return(0);
}

/*
 		#] CloseStream :
 		#[ CreateStream :
*/

STREAM *CreateStream ARG1(UBYTE *,where)
{
	STREAM *newstreams;
	int numnewstreams,i;
	int offset;
	if ( AC.NumStreams >= AC.MaxNumStreams ) {
		if ( AC.MaxNumStreams == 0 ) numnewstreams = 10;
		else                        numnewstreams = 2*AC.MaxNumStreams;
		newstreams = (STREAM *)Malloc1(sizeof(STREAM)*(numnewstreams+1),"CreateStream");
		if ( AC.MaxNumStreams > 0 ) {
			offset = AC.CurrentStream - AC.Streams;
			for ( i = 0; i < AC.MaxNumStreams; i++ ) {
				newstreams[i] = AC.Streams[i];
			}
			AC.CurrentStream = newstreams + offset;
		}
		else newstreams[0].previous = -1;
		AC.MaxNumStreams = numnewstreams;
		if ( AC.Streams ) M_free(AC.Streams,(char *)where);
		AC.Streams = newstreams;
	}
	newstreams = AC.Streams+AC.NumStreams++;
	newstreams->name = 0;
	return(newstreams);
}

/*
 		#] CreateStream :
 		#[ GetStreamPosition :
*/

LONG
GetStreamPosition ARG1(STREAM *,stream)
{
	return(stream->bufferposition + ((LONG)stream->pointer-(LONG)stream->buffer));
}

/*
 		#] GetStreamPosition :
 		#[ PositionStream :
*/

VOID
PositionStream ARG2(STREAM *,stream,LONG,position)
{
	POSITION scrpos;
	if ( position >= stream->bufferposition
	&& position < stream->bufferposition + stream->inbuffer ) {
		stream->pointer = stream->buffer + (position-stream->bufferposition);
	}
	else if ( stream->type == FILESTREAM ) {
		SETBASEPOSITION(scrpos,position);
		SeekFile(stream->handle,&scrpos,SEEK_SET);
		stream->inbuffer = ReadFile(stream->handle,stream->buffer,stream->buffersize);
		stream->pointer = stream->buffer;
		stream->top = stream->buffer + stream->inbuffer;
		stream->bufferposition = position;
		stream->fileposition = position + stream->inbuffer;
		stream->isnextchar = 0;
	}
	else {
		Error0("Illegal position for stream");
		Terminate(-1);
	} 
}

/*
 		#] PositionStream :
  	#] Streams :
  	#[ Files :
 		#[ StartFiles :
*/

VOID
StartFiles ARG0
{
	int i = CreateHandle();
	filelist[i] = Ustdout;
	AM.StdOut = i;
	AC.StoreHandle = -1;
	AC.LogHandle = -1;
#ifndef WITHPTHREADS
	AR.Fscr[0].handle = -1;
	AR.Fscr[1].handle = -1;
	AR.Fscr[2].handle = -1;
	AR.FoStage4[0].handle = -1;
	AR.FoStage4[1].handle = -1;
	AR.infile = &(AR.Fscr[0]);
	AR.outfile = &(AR.Fscr[1]);
	AS.hidefile = &(AR.Fscr[2]);
#endif
	AO.StoreData.Handle = -1;
	AC.Streams = 0;
	AC.MaxNumStreams = 0;
}

/*
 		#] StartFiles :
 		#[ OpenFile :
*/

int
OpenFile ARG1(char *,name)
{
	FILES *f;
	int i;
	if ( ( f = Uopen(name,"rb") ) == 0 ) return(-1);
/*	Usetbuf(f,0); */
	i = CreateHandle();
	filelist[i] = f;
	return(i);
}

/*
 		#] OpenFile :
 		#[ OpenAddFile :
*/

int
OpenAddFile ARG1(char *,name)
{
	FILES *f;
	int i;
	POSITION scrpos;
	if ( ( f = Uopen(name,"a+b") ) == 0 ) return(-1);
/*	Usetbuf(f,0); */
	i = CreateHandle();
	filelist[i] = f;
	TELLFILE(i,&scrpos);
	SeekFile(i,&scrpos,SEEK_SET);
	return(i);
}

/*
 		#] OpenAddFile :
 		#[ CreateFile :
*/

int
CreateFile ARG1(char *,name)
{
	FILES *f;
	int i;
	if ( ( f = Uopen(name,"w+b") ) == 0 ) return(-1);
	i = CreateHandle();
	filelist[i] = f;
	return(i);
}

/*
 		#] CreateFile :
 		#[ CreateLogFile :
*/

int
CreateLogFile ARG1(char *,name)
{
	FILES *f;
	int i;
	if ( ( f = Uopen(name,"w+b") ) == 0 ) return(-1);
	Usetbuf(f,0);
	i = CreateHandle();
	filelist[i] = f;
	return(i);
}

/*
 		#] CreateLogFile :
 		#[ CloseFile :
*/

VOID
CloseFile ARG1(int,handle)
{
	if ( handle >= 0 ) {
		Uclose(filelist[handle]);
		filelist[handle] = 0;
		numinfilelist--;
	}
}

/*
 		#] CloseFile :
 		#[ CreateHandle :
*/

int
CreateHandle ARG0
{
	int i, j;
	if ( filelistsize == 0 ) {
        filelistsize = 10;
        filelist = (FILES **)Malloc1(sizeof(FILES *)*filelistsize,"file handle");
        for ( j = 0; j < filelistsize; j++ ) filelist[j] = 0;
        numinfilelist = 1;
        i = 0;
	}
	else if ( numinfilelist >= filelistsize ) {
		i = filelistsize;
		if ( DoubleList((VOID ***)(&filelist),&filelistsize,(int)sizeof(FILES *),
			"list of open files") != 0 ) Terminate(-1);
		for ( j = i; j < filelistsize; j++ ) filelist[j] = 0;
		numinfilelist = i + 1;
	}
	else {
        i = filelistsize;
        for ( j = 0; j < filelistsize; j++ ) {
            if ( filelist[j] == 0 ) { i = j; break; }
        }
		numinfilelist++;
	}
	if ( numinfilelist > MAX_OPEN_FILES ) {
		MesPrint("More than %d open files",MAX_OPEN_FILES);
		Error0("System limit. This limit is not due to FORM!");
	}
	return(i);
}

/*
 		#] CreateHandle :
 		#[ ReadFile :
*/

LONG
ReadFile ARG3(int,handle,UBYTE *,buffer,LONG,size)
{
	LONG inbuf = 0, r;
	char *b;
	b = (char *)buffer;
	for(;;) {	/* Gotta do difficult because of VMS! */
		r = Uread(b,1,size,filelist[handle]);
		if ( r < 0 ) return(r);
		if ( r == 0 ) return(inbuf);
		inbuf += r;
		if ( r == size ) return(inbuf);
		if ( r > size ) return(-1);
		size -= r;
		b += r;
	}
}

/*
 		#] ReadFile :
 		#[ WriteFile :
*/

LONG
WriteFileToFile ARG3(int,handle,UBYTE *,buffer,LONG,size)
{
	return(Uwrite((char *)buffer,1,size,filelist[handle]));
}

LONG (*WriteFile) ARG3 (int,handle,UBYTE *,buffer,LONG,size) = &WriteFileToFile;

/*
 		#] WriteFile :
 		#[ SeekFile :
*/

VOID
SeekFile ARG3(int,handle,POSITION *,offset,int,origin)
{
	if ( origin == SEEK_SET ) {
		Useek(filelist[handle],BASEPOSITION(*offset),origin);
		SETBASEPOSITION(*offset,(Utell(filelist[handle])));
		return;
	}
	else if ( origin == SEEK_END ) {
		Useek(filelist[handle],0,origin);
	}
	SETBASEPOSITION(*offset,(Utell(filelist[handle])));
}

/*
 		#] SeekFile :
 		#[ TellFile :
*/

LONG
TellFile ARG1(int,handle)
{
	POSITION pos;
	TELLFILE(handle,&pos);
	return(BASEPOSITION(pos));
}

VOID
TELLFILE ARG2(int,handle,POSITION *,position)
{
	SETBASEPOSITION(*position,(Utell(filelist[handle])));
}

/*
 		#] TellFile :
 		#[ FlushFile :
*/

void FlushFile ARG1(int,handle)
{
	Uflush(filelist[handle]);
}

/*
 		#] FlushFile :
 		#[ GetPosFile :
*/

int
GetPosFile ARG2(int,handle,fpos_t *,pospointer)
{
	return(Ugetpos(filelist[handle],pospointer));
}

/*
 		#] GetPosFile :
 		#[ SetPosFile :
*/

int
SetPosFile ARG2(int,handle,fpos_t *,pospointer)
{
	return(Usetpos(filelist[handle],(fpos_t *)pospointer));
}

/*
 		#] SetPosFile :
 		#[ GetChannel :

		Checks whether we have this file already. If so, we return its
		handle. If not, we open the file first and add it to the buffers.
*/

int
GetChannel ARG1(char *,name)
{
	CHANNEL *ch;
	int i;
	for ( i = 0; i < NumOutputChannels; i++ ) {
		if ( channels[i].name == 0 ) continue;
		if ( StrCmp((UBYTE *)name,(UBYTE *)(channels[i].name)) == 0 ) return(channels[i].handle);
	}
	for ( i = 0; i < NumOutputChannels; i++ ) {
		if ( channels[i].name == 0 ) break;
	}
	if ( i < NumOutputChannels ) { ch = &(channels[i]); }
	else { ch = (CHANNEL *)FromList(&AC.ChannelList); }
	ch->name = (char *)strDup1((UBYTE *)name,"name of channel");
	ch->handle = CreateFile(name);
	Usetbuf(filelist[ch->handle],0);	 /* We turn the buffer off!!!!!!*/
	return(ch->handle);
}

/*
 		#] GetChannel :
 		#[ GetAppendChannel :

		Checks whether we have this file already. If so, we return its
		handle. If not, we open the file first and add it to the buffers.
*/

int
GetAppendChannel ARG1(char *,name)
{
	CHANNEL *ch;
	int i;
	for ( i = 0; i < NumOutputChannels; i++ ) {
		if ( channels[i].name == 0 ) continue;
		if ( StrCmp((UBYTE *)name,(UBYTE *)(channels[i].name)) == 0 ) return(channels[i].handle);
	}
	for ( i = 0; i < NumOutputChannels; i++ ) {
		if ( channels[i].name == 0 ) break;
	}
	if ( i < NumOutputChannels ) { ch = &(channels[i]); }
	else { ch = (CHANNEL *)FromList(&AC.ChannelList); }
	ch->name = (char *)strDup1((UBYTE *)name,"name of channel");
	ch->handle = OpenAddFile(name);
	Usetbuf(filelist[ch->handle],0);	 /* We turn the buffer off!!!!!!*/
	return(ch->handle);
}

/*
 		#] GetAppendChannel :
 		#[ CloseChannel :

		Checks whether we have this file already. If so, we close it.
*/

int
CloseChannel ARG1(char *,name)
{
	int i;
	for ( i = 0; i < NumOutputChannels; i++ ) {
		if ( channels[i].name == 0 ) continue;
		if ( channels[i].name[0] == 0 ) continue;
		if ( StrCmp((UBYTE *)name,(UBYTE *)(channels[i].name)) == 0 ) {
			CloseFile(channels[i].handle);
			M_free(channels[i].name,"CloseChannel");
			channels[i].name = 0;
			return(0);
		}
	}
	return(0);
}

/*
 		#] CloseChannel :
  	#] Files :
  	#[ Strings :
 		#[ StrCmp :
*/

int
StrCmp ARG2(UBYTE *,s1,UBYTE *,s2)
{
	while ( *s1 && *s1 == *s2 ) { s1++; s2++; }
	return((int)*s1-(int)*s2);
}

/*
 		#] StrCmp :
 		#[ StrICmp :
*/

int
StrICmp ARG2(UBYTE *,s1,UBYTE *,s2)
{
	while ( *s1 && tolower(*s1) == tolower(*s2) ) { s1++; s2++; }
	return((int)tolower(*s1)-(int)tolower(*s2));
}

/*
 		#] StrICmp :
 		#[ StrHICmp :
*/

int
StrHICmp ARG2(UBYTE *,s1,UBYTE *,s2)
{
	while ( *s1 && tolower(*s1) == *s2 ) { s1++; s2++; }
	return((int)tolower(*s1)-(int)(*s2));
}

/*
 		#] StrHICmp :
 		#[ StrICont :
*/

int
StrICont ARG2(UBYTE *,s1,UBYTE *,s2)
{
	while ( *s1 && tolower(*s1) == tolower(*s2) ) { s1++; s2++; }
	if ( *s1 == 0 ) return(0);
	return((int)tolower(*s1)-(int)tolower(*s2));
}

/*
 		#] StrICont :
 		#[ ConWord :
*/

int
ConWord ARG2(UBYTE *,s1,UBYTE *,s2)
{
	while ( *s1 && tolower(*s1) == tolower(*s2) ) { s1++; s2++; }
	if ( *s1 == 0 ) return(1);
	return(0);
}

/*
 		#] ConWord :
 		#[ StrLen :
*/

int
StrLen ARG1(UBYTE *,s)
{
	int i = 0;
	while ( *s ) { s++; i++; }
	return(i);
}

/*
 		#] StrLen :
 		#[ NumToStr :
*/

VOID
NumToStr ARG2(UBYTE *,s,LONG,x)
{
	UBYTE *t, str[24];
	t = str;
	/*[02dec2004 mt]: the following line may lead to integer overflow!:*/
	if ( x < 0 ) { *s++ = '-'; x = -x; }
	do {
		*t++ = x % 10 + '0';
		x /= 10;
	} while ( x );
	while ( t > str ) *s++ = *--t;
	*s = 0;
}

/*
 		#] NumToStr :
 		#[ WriteString :

		Writes a characterstring to the various outputs.
		The action may depend on the flags involved.
		The type of output is given by type, the string by str and the
		number of characters in it by num
*/
VOID
WriteString ARG3(int,type,UBYTE *,str,int,num)
{
	int error = 0;

	if ( num > 0 && str[num-1] == 0 ) { num--; }
	else if ( num <= 0 || str[num-1] != LINEFEED ) {
		AddLineFeed(str,num);
	}
	/*[15apr2004 mt]:*/
	if(type == EXTERNALCHANNELOUT){
		if(WriteFile(0,str,num) != num) error = 1;
	}else
	/*:[15apr2004 mt]*/
	if ( AM.silent == 0 || type == ERROROUT ) {
		if ( type == INPUTOUT ) {
			if ( !AM.FileOnlyFlag && WriteFile(AM.StdOut,(UBYTE *)"    ",4) != 4 ) error = 1;
			if ( AC.LogHandle >= 0 && WriteFile(AC.LogHandle,(UBYTE *)"    ",4) != 4 ) error = 1;
		}
		if ( !AM.FileOnlyFlag && WriteFile(AM.StdOut,str,num) != num ) error = 1;
		if ( AC.LogHandle >= 0 && WriteFile(AC.LogHandle,str,num) != num ) error = 1;
	}
	if ( error ) Terminate(-1);
}

/*
 		#] WriteString :
 		#[ WriteUnfinString :

		Writes a characterstring to the various outputs.
		The action may depend on the flags involved.
		The type of output is given by type, the string by str and the
		number of characters in it by num
*/

VOID
WriteUnfinString ARG3(int,type,UBYTE *,str,int,num)
{
	int error = 0;

	/*[15apr2004 mt]:*/
	if(type == EXTERNALCHANNELOUT){
		if(WriteFile(0,str,num) != num) error = 1;
	}else
	/*:[15apr2004 mt]*/
	if ( AM.silent == 0 || type == ERROROUT ) {
		if ( type == INPUTOUT ) {
			if ( !AM.FileOnlyFlag && WriteFile(AM.StdOut,(UBYTE *)"    ",4) != 4 ) error = 1;
			if ( AC.LogHandle >= 0 && WriteFile(AC.LogHandle,(UBYTE *)"    ",4) != 4 ) error = 1;
		}
		if ( !AM.FileOnlyFlag && WriteFile(AM.StdOut,str,num) != num ) error = 1;
		if ( AC.LogHandle >= 0 && WriteFile(AC.LogHandle,str,num) != num ) error = 1;
	}
	if ( error ) Terminate(-1);
}

/*
 		#] WriteUnfinString :
 		#[ strDup1 :

		string duplication with message passing for Malloc1, allowing
		this routine to give a more detailed error message if there
		is not enough memory.
*/

UBYTE *
strDup1 ARG2(UBYTE *,instring,char *,ifwrong)
{
	UBYTE *s = instring, *to;
	while ( *s ) s++;
	to = s = (UBYTE *)Malloc1((s-instring)+1,ifwrong);
	while ( *instring ) *to++ = *instring++;
	*to = 0;
	return(s);
}

/*
 		#] strDup1 :
 		#[ EndOfToken :
*/

UBYTE *
EndOfToken ARG1(UBYTE *,s)
{
	UBYTE c;
	while ( ( c = FG.cTable[*s] ) == 0 || c == 1 ) s++;
	return(s);
}

/*
 		#] EndOfToken :
 		#[ ToToken :
*/

UBYTE *
ToToken ARG1(UBYTE *,s)
{
	UBYTE c;
	while ( *s && ( c = FG.cTable[*s] ) != 0 && c != 1 ) s++;
	return(s);
}

/*
 		#] ToToken :
 		#[ SkipField :

	Skips from s to the end of a declaration field.
	par is the number of parentheses that still has to be closed.
*/
 
UBYTE *
SkipField ARG2(UBYTE *,s,int,level)
{
	while ( *s ) {
		if ( *s == ',' && level == 0 ) return(s);
		if ( *s == '(' ) level++;
		else if ( *s == ')' ) { level--; if ( level < 0 ) level = 0; }
		else if ( *s == '[' ) {
			SKIPBRA1(s)
		}
		else if ( *s == '{' ) {
			SKIPBRA2(s)
		}
		s++;
	}
	return(s);
}

/*
 		#] SkipField :
 		#[ ReadSnum :			WORD ReadSnum(p)

		Reads a number that should fit in a word.
		The number should be unsigned and a negative return value
		indicates an irregularity.

*/

WORD
ReadSnum ARG1(UBYTE **,p)
{
	LONG x = 0;
	UBYTE *s;
	s = *p;
	if ( FG.cTable[*s] == 1 ) {
		do {
			x = ( x << 3 ) + ( x << 1 ) + ( *s++ - '0' );
			if ( x > MAXPOSITIVE ) return(-1);
		} while ( FG.cTable[*s] == 1 );
		*p = s;
		return((WORD)x);
	}
	else return(-1);
}

/*
 		#] ReadSnum :
 		#[ NumCopy :

	Adds the decimal representation of a number to a string.

*/

UBYTE *
NumCopy ARG2(WORD,x,UBYTE *,to)
{
	UBYTE *s;
	WORD i = 0, j;
	if ( x < 0 ) { x = -x; *to++ = '-'; }
	s = to;
	do { *s++ = (x % 10)+'0'; i++; } while ( ( x /= 10 ) != 0 );
	*s-- = '\0';
	j = ( i - 1 ) >> 1;
	while ( j >= 0 ) {
		i = to[j]; to[j] = s[-j]; s[-j] = i; j--;
	}
	return(s+1);
}

/*
 		#] NumCopy :
 		#[ LongCopy :

	Adds the decimal representation of a number to a string.

*/

char *
LongCopy ARG2(LONG,x,char *,to)
{
	char *s;
	WORD i = 0, j;
	if ( x < 0 ) { x = -x; *to++ = '-'; }
	s = to;
	do { *s++ = (x % 10)+'0'; i++; } while ( ( x /= 10 ) != 0 );
	*s-- = '\0';
	j = ( i - 1 ) >> 1;
	while ( j >= 0 ) {
		i = to[j]; to[j] = s[-j]; s[-j] = i; j--;
	}
	return(s+1);
}

/*
 		#] LongCopy :
 		#[ LongLongCopy :

	Adds the decimal representation of a number to a string.
	Bugfix feb 2003. y was not pointer!
*/

char *
LongLongCopy ARG2(off_t *,y,char *,to)
{
	off_t x = *y;
	char *s;
	WORD i = 0, j;
	if ( x < 0 ) { x = -x; *to++ = '-'; }
	s = to;
	do { *s++ = (x % 10)+'0'; i++; } while ( ( x /= 10 ) != 0 );
	*s-- = '\0';
	j = ( i - 1 ) >> 1;
	while ( j >= 0 ) {
		i = to[j]; to[j] = s[-j]; s[-j] = i; j--;
	}
	return(s+1);
}

/*
 		#] LongLongCopy :
 		#[ MakeDate :

		Routine produces a string with the date and time of the run
*/

#ifdef ANSI
#else
#ifdef mBSD
#else
static char notime[] = "";
#endif
#endif

UBYTE *
MakeDate ARG0
{
#ifdef ANSI
	time_t tp;
	time(&tp);
	return((UBYTE *)ctime(&tp));
#else
#ifdef mBSD
	time_t tp;
	time(&tp);
	return((UBYTE *)ctime(&tp));
#else
	return((UBYTE *)notime);
#endif
#endif
}

/*
 		#] MakeDate :
  	#] Strings :
  	#[ Mixed :
 		#[ Malloc :

		Malloc routine with built in error checking.
		This saves lots of messages.
*/
#ifdef MALLOCDEBUG
char *dummymessage = "Malloc";
INILOCK(MallocLock);
#endif
 
VOID *
Malloc ARG1(LONG,size)
{
	VOID *mem;
#ifdef MALLOCDEBUG
	char *t, *u;
	int i;
	LOCK(MallocLock);
	LOCK(ErrorMessageLock);
	if ( size == 0 ) {
		MesPrint("Asking for 0 bytes in Malloc");
	}
#endif
	if ( ( size & 7 ) != 0 ) { size = size - ( size&7 ) + 8; }
#ifdef MALLOCDEBUG
	size += 2*BANNER;
#endif
	mem = (VOID *)M_alloc(size);
	if ( mem == 0 ) {
#ifndef MALLOCDEBUG
		LOCK(ErrorMessageLock);
#else
		UNLOCK(MallocLock);
#endif
		Error0("No memory!");
		UNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
#ifdef MALLOCDEBUG
	mallocsizes[nummalloclist] = size;
	mallocstrings[nummalloclist] = dummymessage;
	malloclist[nummalloclist++] = mem;
	if ( filelist ) MesPrint("Mem0 at 0x%x, %l bytes",mem,size);
	{
		int i = nummalloclist-1;
		while ( --i >= 0 ) {
			if ( (char *)mem < (((char *)malloclist[i]) + mallocsizes[i])
			&& (char *)(malloclist[i]) < ((char *)mem + size) ) {
				if ( filelist ) MesPrint("This memory overlaps with the block at 0x%x"
					,malloclist[i]);
			}
		}
	}
	t = (char *)mem;
	u = t + size;
	for ( i = 0; i < BANNER; i++ ) { *t++ = 0; *--u = 0; }
	mem = (void *)t;
	{
		int j = nummalloclist-1, i;
		while ( --j >= 0 ) {
			t = (char *)(malloclist[j]);
			u = t + mallocsizes[j];
			for ( i = 0; i < BANNER; i++ ) {
				u--;
				if ( *t != 0 || *u != 0 ) {
					MesPrint("Writing outside memory for %s",malloclist[i]);
					UNLOCK(ErrorMessageLock);
					UNLOCK(MallocLock);
					Terminate(-1);
				}
				t--;
			}
		}
	}
	UNLOCK(ErrorMessageLock);
	UNLOCK(MallocLock);
#endif
	return(mem);
}

/*
 		#] Malloc :
 		#[ Malloc1 :

		Malloc with more detailed error message.
		Gives the user some idea of what is happening.
*/

VOID *
Malloc1 ARG2(LONG,size,char *,messageifwrong)
{
	VOID *mem;
#ifdef MALLOCDEBUG
	char *t, *u;
	int i;
	LOCK(MallocLock);
	LOCK(ErrorMessageLock);
	if ( size == 0 ) {
		MesPrint("Asking for 0 bytes in Malloc1");
	}
#endif
	if ( ( size & 7 ) != 0 ) { size = size - ( size&7 ) + 8; }
#ifdef MALLOCDEBUG
	size += 2*BANNER;
#endif
	mem = (VOID *)M_alloc(size);
	if ( mem == 0 ) {
#ifndef MALLOCDEBUG
		LOCK(ErrorMessageLock);
#else
		UNLOCK(MallocLock);
#endif
		Error1("No memory while allocating ",(UBYTE *)messageifwrong);
		UNLOCK(ErrorMessageLock);
		Terminate(-1);
	}
#ifdef MALLOCDEBUG
	mallocsizes[nummalloclist] = size;
	mallocstrings[nummalloclist] = (char *)messageifwrong;
	malloclist[nummalloclist++] = mem;
	if ( filelist ) MesPrint("Mem1 at 0x%x: %l bytes. %s",mem,size,messageifwrong);
	{
		int i = nummalloclist-1;
		while ( --i >= 0 ) {
			if ( (char *)mem < (((char *)malloclist[i]) + mallocsizes[i])
			&& (char *)(malloclist[i]) < ((char *)mem + size) ) {
				if ( filelist ) MesPrint("This memory overlaps with the block at 0x%x"
					,malloclist[i]);
			}
		}
	}
	t = (char *)mem;
	u = t + size;
	for ( i = 0; i < BANNER; i++ ) { *t++ = 0; *--u = 0; }
	mem = (void *)t;
	M_check();
	UNLOCK(ErrorMessageLock);
	UNLOCK(MallocLock);
#endif
	return(mem);
}

/*
 		#] Malloc1 :
 		#[ M_free :
*/

void M_free ARG2(VOID *,x,char *,where)
{
#ifdef MALLOCDEBUG
	char *t = (char *)x;
	int i, j, k;
	LONG size = 0;
	LOCK(MallocLock);
	LOCK(ErrorMessageLock);
	x = (void *)(((char *)x)-BANNER);
	MesPrint("Freeing 0x%x: %s",x,where);
	for ( i = nummalloclist-1; i >= 0; i-- ) {
		if ( x == malloclist[i] ) {
			size = mallocsizes[i];
			for ( j = i+1; j < nummalloclist; j++ ) {
				malloclist[j-1] = malloclist[j];
				mallocsizes[j-1] = mallocsizes[j];
				mallocstrings[j-1] = mallocstrings[j];
			}
			nummalloclist--;
			break;
		}
	}
	if ( i < 0 ) {
		printf("Error returning non-allocated address: 0x%x from %s\n"
			,(unsigned int)x,where);
		UNLOCK(ErrorMessageLock);
		UNLOCK(MallocLock);
		exit(-1);
	}
	else {
		for ( k = 0, j = 0; k < BANNER; k++ ) {
			if ( *--t ) j++;
		}
		if ( j ) {
			LONG *tt = (LONG *)x;
			MesPrint("!!!!! Banner has been written in !!!!!: %x %x %x %x",
			tt[0],tt[1],tt[2],tt[3]);
		}
		t += size;
		for ( k = 0, j = 0; k < BANNER; k++ ) {
			if ( *--t ) j++;
		}
		if ( j ) {
			LONG *tt = (LONG *)x;
			MesPrint("!!!!! Tail has been written in !!!!!: %x %x %x %x",
			tt[0],tt[1],tt[2],tt[3]);
		}
		M_check();
		UNLOCK(ErrorMessageLock);
		UNLOCK(MallocLock);
	}
#endif
	if ( x ) free(x);
}

/*
 		#] M_free :
 		#[ M_check :
*/

#ifdef MALLOCDEBUG

void M_check1() { MesPrint("Checking Malloc"); M_check(); }

void M_check()
{
	int i,j,k,error = 0;
	char *t;
	LONG *tt;
	for ( i = 0; i < nummalloclist; i++ ) {
		t = (char *)(malloclist[i]);
		for ( k = 0, j = 0; k < BANNER; k++ ) {
			if ( *t++ ) j++;
		}
		if ( j ) {
			tt = (LONG *)(malloclist[i]);
			MesPrint("!!!!! Banner %d (%s) has been written in !!!!!: %x %x %x %x",
			i,mallocstrings[i],tt[0],tt[1],tt[2],tt[3]);
			tt[0] = tt[1] = tt[2] = tt[3] = 0;
			error = 1;
		}
		t = (char *)(malloclist[i]) + mallocsizes[i];
		for ( k = 0, j = 0; k < BANNER; k++ ) {
			if ( *--t ) j++;
		}
		if ( j ) {
			tt = (LONG *)t;
			MesPrint("!!!!! Tail %d (%s) has been written in !!!!!: %x %x %x %x",
			i,mallocstrings[i],tt[0],tt[1],tt[2],tt[3]);
			tt[0] = tt[1] = tt[2] = tt[3] = 0;
			error = 1;
		}
		if ( ( mallocstrings[i][0] == ' ' ) || ( mallocstrings[i][0] == '#' ) ) {
			MesPrint("!!!!! Funny mallocstring");
			error = 1;
		}
	}
	if ( error ) {
		M_print();
		UNLOCK(ErrorMessageLock);
		UNLOCK(MallocLock);
		Terminate(-1);
	}
}

void M_print ARG0
{
	int i;
	MesPrint("We have the following memory allocations left:");
	for ( i = 0; i < nummalloclist; i++ ) {
		MesPrint("0x%x: %l bytes. number %d: '%s'",malloclist[i],mallocsizes[i],i,mallocstrings[i]);
	}
}

#else

void M_check1() {}
void M_print() {}

#endif

/*
 		#] M_check :
 		#[ FromList :

	Returns the next object in a list.
	If the list has been exhausted we double it (like a realloc)
	If the list has not been initialized yet we start with 10 elements.
*/

VOID *
FromList ARG1(LIST *,L)
{
	void *newlist;
	int i, *old, *newL;
	if ( L->num >= L->maxnum || L->lijst == 0 ) {
		if ( L->maxnum == 0 ) L->maxnum = 12;
		else if ( L->lijst ) L->maxnum *= 2;
		newlist = Malloc1(L->maxnum * L->size,L->message);
		if ( L->lijst ) {
			i = ( L->num * L->size ) / sizeof(int);
			old = (int *)L->lijst; newL = (int *)newlist;
			while ( --i >= 0 ) *newL++ = *old++;
			if ( L->lijst ) M_free(L->lijst,"L->lijst FromList");
		}
		L->lijst = newlist;
	}
	return( ((char *)(L->lijst)) + L->size * (L->num)++ );
}

/*
 		#] FromList :
 		#[ From0List :

		Same as FromList, but we zero excess variables.
*/

VOID *
From0List ARG1(LIST *,L)
{
	void *newlist;
	int i, *old, *newL;
	if ( L->num >= L->maxnum || L->lijst == 0 ) {
		if ( L->maxnum == 0 ) L->maxnum = 12;
		else if ( L->lijst ) L->maxnum *= 2;
		newlist = Malloc1(L->maxnum * L->size,L->message);
		i = ( L->num * L->size ) / sizeof(int);
		old = (int *)(L->lijst); newL = (int *)newlist;
		while ( --i >= 0 ) *newL++ = *old++;
		i = ( L->maxnum - L->num ) / sizeof(int);
		while ( --i >= 0 ) *newL++ = 0;
		if ( L->lijst ) M_free(L->lijst,"L->lijst From0List");
		L->lijst = newlist;
	}
	return( ((char *)(L->lijst)) + L->size * (L->num)++ );
}

/*
 		#] From0List :
 		#[ FromVarList :

	Returns the next object in a list of variables.
	If the list has been exhausted we double it (like a realloc)
	If the list has not been initialized yet we start with 10 elements.
	We allow at most MAXVARIABLES elements!
*/

VOID *
FromVarList ARG1(LIST *,L)
{
	void *newlist;
	int i, *old, *newL;
	if ( L->num >= L->maxnum || L->lijst == 0 ) {
		if ( L->maxnum == 0 ) L->maxnum = 12;
		else if ( L->lijst ) {
			L->maxnum *= 2;
			if ( L->maxnum > MAXVARIABLES ) L->maxnum = MAXVARIABLES;
			if ( L->num >= MAXVARIABLES ) {
				MesPrint("!!!More than %l objects in list of variables",
					MAXVARIABLES);
				Terminate(-1);
			}
		}
		newlist = Malloc1(L->maxnum * L->size,L->message);
		if ( L->lijst ) {
			i = ( L->num * L->size ) / sizeof(int);
			old = (int *)(L->lijst); newL = (int *)newlist;
			while ( --i >= 0 ) *newL++ = *old++;
			if ( L->lijst ) M_free(L->lijst,"L->lijst from VarList");
		}
		L->lijst = newlist;
	}
	return( ((char *)(L->lijst)) + L->size * ((L->num)++) );
}

/*
 		#] FromVarList :
 		#[ DoubleList :

		To make the system thread safe we set maxlistsize fixed to MAXPOSITIVE
*/

int
DoubleList ARG4(VOID ***,lijst,int *,oldsize,int,objectsize,char *,nameoftype)
{
	int error;
	LONG lsize = *oldsize;
/*
	maxlistsize = (LONG)(MAXPOSITIVE);
*/
	error = DoubleLList(lijst,&lsize,objectsize,nameoftype);
	*oldsize = lsize;
/*
	maxlistsize = (LONG)(MAXLONG);
*/
	return(error);
}

/*
 		#] DoubleList :
 		#[ DoubleLList :
*/

int
DoubleLList ARG4(VOID ***,lijst,LONG *,oldsize,int,objectsize,char *,nameoftype)
{
	VOID **newlist;
	LONG i, newsize, fullsize;
	VOID **to, **from;
	if ( *lijst == 0 ) {
		if ( *oldsize > 0 ) newsize = *oldsize;
		else newsize = 100;
	}
	else newsize = *oldsize * 2;
	if ( newsize > maxlistsize ) {
		if ( *oldsize == maxlistsize ) {
			MesPrint("No memory for extra space in %s",nameoftype);
			return(-1);
		}
		newsize = maxlistsize;
	}
	fullsize = ( newsize * objectsize + sizeof(VOID *)-1 ) & (-sizeof(VOID *));
	newlist = (VOID **)Malloc1(fullsize,nameoftype);
	if ( *lijst ) {	/* Now some punning. DANGEROUS CODE in principle */
		to = newlist; from = *lijst; i = (*oldsize * objectsize)/sizeof(VOID *);
/*
#ifdef MALLOCDEBUG
if ( filelist ) MesPrint("    oldsize: %l, objectsize: %d, fullsize: %l"
		,*oldsize,objectsize,fullsize);
#endif
*/
		while ( --i >= 0 ) *to++ = *from++;
	}
	if ( *lijst ) M_free(*lijst,"DoubleLList");
	*lijst = newlist;
	*oldsize = newsize;
	return(0);
}

/*
 		#] DoubleLList :
 		#[ DoubleBuffer :
*/

#define DODOUBLE(x) { x *s, *t, *u; if ( *start ) { \
	oldsize = *(x **)stop - *(x **)start; newsize = 2*oldsize; \
	t = u = (x *)Malloc1(newsize*sizeof(x),text); s = *(x **)start; \
	for ( i = 0; i < oldsize; i++ ) *t++ = *s++; M_free(*start,"double"); } \
	else { newsize = 100; u = (x *)Malloc1(newsize*sizeof(x),text); } \
	*start = (void *)u; *stop = (void *)(u+newsize); }

void
DoubleBuffer ARG4(void **,start,void **,stop,int,size,char *,text)
{
	long oldsize, newsize, i;
	if ( size == sizeof(char) ) DODOUBLE(char)
	else if ( size == sizeof(short) ) DODOUBLE(short)
	else if ( size == sizeof(int) ) DODOUBLE(int)
	else if ( size == sizeof(long) ) DODOUBLE(long)
	else if ( size % sizeof(int) == 0 ) DODOUBLE(int)
	else {
		MesPrint("---Cannot handle doubling buffers of size %d",size);
		Terminate(-1);
	}
}

/*
 		#] DoubleBuffer :
 		#[ ExpandBuffer :
*/

#define DOEXPAND(x) { x *newbuffer, *t, *m;                             \
	t = newbuffer = (x *)Malloc1((newsize+2)*type,"ExpandBuffer");      \
	if ( *buffer ) { m = (x *)*buffer; i = *oldsize;                    \
		while ( --i >= 0 ) *t++ = *m++; M_free(*buffer,"ExpandBuffer"); \
	} *buffer = newbuffer; *oldsize = newsize; }

void ExpandBuffer ARG3(void **,buffer,LONG *,oldsize,int,type)
{
	LONG newsize, i;
	if ( *oldsize <= 0 ) { newsize = 100; }
	else newsize = 2*(*oldsize);
	if ( type == sizeof(char) ) DOEXPAND(char)
	else if ( type == sizeof(short) ) DOEXPAND(short)
	else if ( type == sizeof(int) ) DOEXPAND(int)
	else if ( type == sizeof(long) ) DOEXPAND(long)
	else if ( type == sizeof(POSITION) ) DOEXPAND(POSITION)
	else {
		MesPrint("---Cannot handle expanding buffers with objects of size %d",type);
		Terminate(-1);
	}
}

/*
 		#] ExpandBuffer :
 		#[ iexp :

		Raises the long integer y to the power p.
		Returnvalue is long, regardless of overflow.
*/

LONG
iexp ARG2(LONG,x,int,p)
{
	int sign;
	LONG y;
	if ( x == 0 ) return(0);
	if ( p == 0 ) return(1);
	if ( x < 0 ) { sign = -1; x = -x; }
	else sign = 1;
	if ( sign < 0 && ( p & 1 ) == 0 ) sign = 1;
	if ( x == 1 ) return(sign);
	if ( p < 0 ) return(0);
	y = 1;
	while ( p ) {
		if ( ( p & 1 ) != 0 ) y *= x;
		p >>= 1;
		x = x*x;
	}
	if ( sign < 0 ) y = -y;
	return(y);
}

/*
 		#] iexp :
 		#[ ToGeneral :

		Convert a fast argument to a general argument
		Input in r, output in m.
		If par == 0 we need the argument header also.
*/

void
ToGeneral ARG3(WORD *,r,WORD *,m,WORD,par)
{
	WORD *mm = m, j, k;
	if ( par ) m++;
	else m += ARGHEAD + 1;
	j = -*r++;
	k = 3;
	if ( j >= FUNCTION ) { *m++ = j; *m++ = 2; }
	else {
		switch ( j ) {
			case SYMBOL: *m++ = j; *m++ = 4; *m++ = *r++; *m++ = 1; break;
			case SNUMBER:
				if ( *r > 0 ) { *m++ =  *r; *m++ = 1; *m++ =  3; }
				else if ( *r == 0 ) { m--; }
				else          { *m++ = -*r; *m++ = 1; *m++ = -3; }
				goto MakeSize;
			case MINVECTOR: k = -k;
			case INDEX:
			case VECTOR: *m++ = INDEX; *m++ = 3; *m++ = *r++; break;
		}
	}
	*m++ = 1; *m++ = 1; *m++ = k;
MakeSize:
	*mm = m-mm;
	if ( !par ) mm[ARGHEAD] = *mm-ARGHEAD;
}

/*
 		#] ToGeneral :
 		#[ ToFast :

		Checks whether an argument can be converted to fast notation
		If this can be done it does it.
		Important: m should be allowed to be equal to r!
		Return value is 1 if conversion took place.
		If there was conversion the answer is in m.
		If there was no conversion m hasn't been touched.
*/

int
ToFast ARG2(WORD *,r,WORD *,m)
{
	WORD i;
	if ( *r == ARGHEAD ) { *m++ = -SNUMBER; *m++ = 0; return(1); }
	if ( *r != r[ARGHEAD]+ARGHEAD ) return(0);	/* > 1 term */
	r += ARGHEAD;
	if ( *r == 4 ) {
		if ( r[2] != 1 || r[1] <= 0 ) return(0);
		*m++ = -SNUMBER; *m = ( r[3] < 0 ) ? -r[1] : r[1]; return(1);
	}
	i = *r - 1;
	if ( r[i-1] != 1 || r[i-2] != 1 ) return(0);
	if ( r[i] != 3 ) {
		if ( r[i] == -3 && r[2] == *r-4 && r[2] == 3 && r[1] == INDEX
		&& r[3] < MINSPEC ) {}
		else return(0);
	}
	else if ( r[2] != *r - 4 ) return(0);
	r++;
	if ( *r >= FUNCTION ) {
		if ( r[1] <= FUNHEAD ) { *m++ = -*r; return(1); }
	}
	else if ( *r == SYMBOL ) {
		if ( r[1] == 4 && r[3] == 1 )
			{ *m++ = -SYMBOL; *m++ = r[2]; return(1); }
	}
	else if ( *r == INDEX ) {
		if ( r[1] == 3 ) {
			if ( r[2] >= MINSPEC ) {
				if ( r[2] >= 0 && r[2] < AM.OffsetIndex ) *m++ = -SNUMBER;
				else *m++ = -INDEX;
			}
			else {
				if ( r[5] == -3 ) *m++ = -MINVECTOR;
				else *m++ = -VECTOR;
			}
			*m++ = r[2];
			return(1);
		}
	}
	return(0);
}

/*
 		#] ToFast :
 		#[ IsLikeVector :

		Routine determines whether a function argument is like a vector.
		Returnvalue: 1: is vector or index
		             0: is not vector or index
		            -1: may be an index
*/

int
IsLikeVector ARG1(WORD *,arg)
{
	WORD *sstop, *t, *tstop;
	if ( *arg < 0 ) {
		if ( *arg == -VECTOR || *arg == -INDEX ) return(1);
		if ( *arg == -SNUMBER && arg[1] >= 0 && arg[1] < AM.OffsetIndex )
			return(-1);
		return(0);
	}
	sstop = arg + *arg; arg += ARGHEAD;
	while ( arg < sstop ) {
		t = arg + *arg;
		tstop = t - ABS(t[-1]);
		arg++;
		while ( arg < tstop ) {
			if ( *arg == INDEX ) return(1);
			arg += arg[1];
		}
		arg = t;
	}
	return(0);
}

/*
 		#] IsLikeVector :
 		#[ CompareArgs :
*/

int
CompareArgs ARG2(WORD *,arg1,WORD *,arg2)
{
	int i1,i2;
	if ( *arg1 > 0 ) {
		if ( *arg2 < 0 ) return(-1);
		i1 = *arg1-ARGHEAD; arg1 += ARGHEAD;
		i2 = *arg2-ARGHEAD; arg2 += ARGHEAD;
		while ( i1 > 0 && i2 > 0 ) {
			if ( *arg1 != *arg2 ) return((int)(*arg1)-(int)(*arg2));
			i1--; i2--; arg1++; arg2++;
		}
		return(i1-i2);
	}
	else if ( *arg2 > 0 ) return(1);
	else {
		if ( *arg1 != *arg2 ) {
			if ( *arg1 < *arg2 ) return(-1);
			else return(1);
		}
		if ( *arg1 <= -FUNCTION ) return(0);
		return((int)(arg1[1])-(int)(arg2[1]));
	}
}

/*
 		#] CompareArgs :
 		#[ CompArg :

	returns 1 if arg1 comes first, -1 if arg2 comes first, 0 if equal
*/

int CompArg ARG2(WORD *,s1, WORD *,s2)
{
	GETIDENTITY;
	WORD *st1, *st2, x[7];
	int k;
	if ( *s1 < 0 ) {
		if ( *s2 < 0 ) {
			if ( *s1 > *s2 ) return(1);
			if ( *s1 < *s2 ) return(-1);
			if ( *s1 <= -FUNCTION ) return(0);
			s1++; s2++;
			if ( *s1 > *s2 ) return(1);
			if ( *s1 < *s2 ) return(-1);
			return(0);
		}
		x[1] = AT.comsym[3];
		x[2] = AT.comnum[1];
		x[3] = AT.comnum[3];
		x[4] = AT.comind[3];
		x[5] = AT.comind[6];
		x[6] = AT.comfun[1];
		if ( *s1 == -SYMBOL ) {
			AT.comsym[3] = s1[1];
			st1 = AT.comsym+8; s1 = AT.comsym;
		}
		else if ( *s1 == -SNUMBER ) {
			if ( s1[1] < 0 ) {
				AT.comnum[1] = -s1[1]; AT.comnum[3] = -3;
			}
			else {
				AT.comnum[1] = s1[1]; AT.comnum[3] = 3;
			}
			st1 = AT.comnum+4;
			s1 = AT.comnum;
		}
		else if ( *s1 == -INDEX || *s1 == -VECTOR ) {
			AT.comind[3] = s1[1]; AT.comind[6] = 3;
			st1 = AT.comind+7; s1 = AT.comind;
		}
		else if ( *s1 == -MINVECTOR ) {
			AT.comind[3] = s1[1]; AT.comind[6] = -3;
			st1 = AT.comind+7; s1 = AT.comind;
		}
		else if ( *s1 <= -FUNCTION ) {
			AT.comfun[1] = -*s1;
			st1 = AT.comfun+FUNHEAD+4; s1 = AT.comfun;
		}
/*
			Symmetrize during compilation of id statement when properorder
			needs this one. Code added 10-nov-2001
*/
		else if ( *s1 == -ARGWILD ) {
			 return(-1);
		}
		else { goto argerror; }
		st2 = s2 + *s2; s2 += ARGHEAD;
		goto docompare;
	}
	else if ( *s2 < 0 ) {
		x[1] = AT.comsym[3];
		x[2] = AT.comnum[1];
		x[3] = AT.comnum[3];
		x[4] = AT.comind[3];
		x[5] = AT.comind[6];
		x[6] = AT.comfun[1];
		if ( *s2 == -SYMBOL ) {
			AT.comsym[3] = s2[1];
			st2 = AT.comsym+8; s2 = AT.comsym;
		}
		else if ( *s2 == -SNUMBER ) {
			if ( s2[1] < 0 ) {
				AT.comnum[1] = -s2[1]; AT.comnum[3] = -3;
				st2 = AT.comnum+4;
			}
			else if ( s2[1] == 0 ) {
				st2 = AT.comnum+4; s2 = st2;
			}
			else {
				AT.comnum[1] = s2[1]; AT.comnum[3] = 3;
				st2 = AT.comnum+4;
			}
			s2 = AT.comnum;
		}
		else if ( *s2 == -INDEX || *s2 == -VECTOR ) {
			AT.comind[3] = s2[1]; AT.comind[6] = 3;
			st2 = AT.comind+7; s2 = AT.comind;
		}
		else if ( *s2 == -MINVECTOR ) {
			AT.comind[3] = s2[1]; AT.comind[6] = -3;
			st2 = AT.comind+7; s2 = AT.comind;
		}
		else if ( *s2 <= -FUNCTION ) {
			AT.comfun[1] = -*s2;
			st2 = AT.comfun+FUNHEAD+4; s2 = AT.comfun;
		}
/*
			Symmetrize during compilation of id statement when properorder
			needs this one. Code added 10-nov-2001
*/
		else if ( *s2 == -ARGWILD ) {
			 return(1);
		}
		else { goto argerror; }
		st1 = s1 + *s1; s1 += ARGHEAD;
		goto docompare;
	}
	else {
		x[1] = AT.comsym[3];
		x[2] = AT.comnum[1];
		x[3] = AT.comnum[3];
		x[4] = AT.comind[3];
		x[5] = AT.comind[6];
		x[6] = AT.comfun[1];
		st1 = s1 + *s1; st2 = s2 + *s2;
		s1 += ARGHEAD; s2 += ARGHEAD;
docompare:
		while ( s1 < st1 && s2 < st2 ) {
			if ( ( k = Compare(s1,s2,(WORD)2) ) != 0 ) {
				AT.comsym[3] = x[1];
				AT.comnum[1] = x[2];
				AT.comnum[3] = x[3];
				AT.comind[3] = x[4];
				AT.comind[6] = x[5];
				AT.comfun[1] = x[6];
				return(-k);
			}
			s1 += *s1; s2 += *s2;
		}
		AT.comsym[3] = x[1];
		AT.comnum[1] = x[2];
		AT.comnum[3] = x[3];
		AT.comind[3] = x[4];
		AT.comind[6] = x[5];
		AT.comfun[1] = x[6];
		if ( s1 < st1 ) return(1);
		if ( s2 < st2 ) return(-1);
	}
	return(0);
 
argerror:
	MesPrint("Illegal type of short function argument in Normalize");
	Terminate(-1); return(0);
}

/*
 		#] CompArg :
 		#[ TimeCPU :
*/

LONG
TimeCPU ARG1(WORD,par)
{
	if ( par ) return(Timer()-AM.OldTime);
	AM.OldTime = Timer();
	return(0L);
}

/*
 		#] TimeCPU :
 		#[ Timer :
*/

#ifdef WINDOWS

#include <time.h>

LONG
Timer()
{
	long t;
	t = clock();
	if ( ( AO.wrapnum & 1 ) != 0 ) t ^= 0x80000000;
	if ( t < 0 ) {
		t ^= 0x80000000;
		AO.wrapnum++;
		AO.wrap += 2147584;
	}
	return(AO.wrap+(t/1000));
}

#else

#ifdef SUN
#define _TIME_T_
#include <sys/time.h>
#include <sys/resource.h>

LONG
Timer ARG0
{
    struct rusage rusage;
    getrusage(0,&rusage);
    return(rusage.ru_utime.tv_sec*1000+rusage.ru_utime.tv_usec/1000);
}

#else

#ifdef LINUX
#include <sys/time.h>
#include <sys/resource.h>

LONG Timer()
{
	struct rusage rusage;
	getrusage(0,&rusage);
	return(rusage.ru_utime.tv_sec*1000+rusage.ru_utime.tv_usec/1000);
}

#else

#ifdef OPTERON
#include <sys/time.h>
#include <sys/resource.h>

LONG Timer()
{
	struct rusage rusage;
	getrusage(0,&rusage);
	return(rusage.ru_utime.tv_sec*1000+rusage.ru_utime.tv_usec/1000);
}

#else

#ifdef RS6K
#include <sys/time.h>
#include <sys/resource.h>

LONG Timer()
{
        struct rusage rusage;
        getrusage(0,&rusage);
        return(rusage.ru_utime.tv_sec*1000+rusage.ru_utime.tv_usec/1000);
}

#else

#ifdef ANSI
LONG
Timer ARG0
{
#ifdef ALPHA
/* 	clock_t t,tikken = clock();									  */
/* 	MesPrint("ALPHA-clock = %l",(long)tikken);					  */
/* 	t = tikken % CLOCKS_PER_SEC;								  */
/* 	tikken /= CLOCKS_PER_SEC;									  */
/* 	tikken *= 1000;												  */
/* 	tikken += (t*1000)/CLOCKS_PER_SEC;							  */
/* 	return((LONG)tikken);										  */
/* #define _TIME_T_												  */
#include <sys/time.h>
#include <sys/resource.h>
    struct rusage rusage;
    getrusage(0,&rusage);
    return(rusage.ru_utime.tv_sec*1000+rusage.ru_utime.tv_usec/1000);
#else
#ifdef DEC_STATION
	clock_t tikken = clock();
	return((LONG)tikken/1000);
#else
	clock_t t, tikken = clock();
	t = tikken % CLK_TCK;
	tikken /= CLK_TCK;
	tikken *= 1000;
	tikken += (t*1000)/CLK_TCK;
	return(tikken);
#endif
#endif
}
#else

#ifdef VMS

#include <time.h>
void times(tbuffer_t *buffer);

LONG
Timer()
{
	tbuffer_t buffer;
	times(&buffer);
	return(buffer.proc_user_time * 10);
}

#endif
#ifdef mBSD

#ifdef MICROTIME
/*
	There is only a CP time clock in microseconds here
	This can cause problems with AO.wrap around
*/
#else
#ifdef mBSD2
#include <sys/types.h>
#include <sys/times.h>
#include <time.h>
long pretime = 0;
#else
#define _TIME_T_
#include <sys/time.h>
#include <sys/resource.h>
#endif
#endif

LONG Timer()
{
#ifdef MICROTIME
	long t;
	t = clock();
	if ( ( AO.wrapnum & 1 ) != 0 ) t ^= 0x80000000;
	if ( t < 0 ) {
		t ^= 0x80000000;
		warpnum++;
		AO.wrap += 2147584;
	}
	return(AO.wrap+(t/1000));
#else
#ifdef mBSD2
	struct tms buffer;
	long ret;
	unsigned long a1, a2, a3, a4;
	times(&buffer);
	a1 = (unsigned long)buffer.tms_utime;
	a2 = a1 >> 16;
	a3 = a1 & 0xFFFFL;
	a3 *= 1000;
	a2 = 1000*a2 + (a3 >> 16);
	a3 &= 0xFFFFL;
	a4 = a2/CLK_TCK;
	a2 %= CLK_TCK;
	a3 += a2 << 16;
	ret = (long)((a4 << 16) + a3 / CLK_TCK);
/*	ret = ((long)buffer.tms_utime * 1000)/CLK_TCK; */
	return(ret);
#else
#ifdef REALTIME
	struct timeval tp;
	struct timezone tzp;
	gettimeofday(&tp,&tzp); */
	return(tp.tv_sec*1000+tp.tv_usec/1000);
#else
	struct rusage rusage;
	getrusage(0,&rusage);
	return(rusage.ru_utime.tv_sec*1000+rusage.ru_utime.tv_usec/1000);
#endif
#endif
#endif
}

#endif
#endif
#endif
#endif
#endif
#endif
#endif

/*
 		#] Timer :
  	#] Mixed :
*/
/*[12dec2003 mt]:*/
/*Four functions to manipulate character sets*/

/*Here it is assumed that there are exactly 8 bits in one 
byte and character occupies exactly one byte.*/

/*
 		#[ set_in :
         Returns 1 if ch is in set ; 0 if ch is not in set:
*/
int 
set_in ARG2(UBYTE, ch, set_of_char, set)
{
	set += ch/8;
	switch (ch % 8){
		case 0: return(set->bit_0);
		case 1: return(set->bit_1);
		case 2: return(set->bit_2);
		case 3: return(set->bit_3);
		case 4: return(set->bit_4);
		case 5: return(set->bit_5);
		case 6: return(set->bit_6);
		case 7: return(set->bit_7);
	}/*switch (ch % 8)*/
	return(-1);
}/*set_in*/
/*
 		#] set_in :
 		#[ set_set :
			sets ch into set; returns *set:
*/
one_byte 
set_set ARG2(UBYTE, ch, set_of_char, set)
{
	one_byte tmp=(one_byte)set;
	set += ch/8;
	switch (ch % 8){
		case 0: set->bit_0=1;break;
		case 1: set->bit_1=1;break;
		case 2: set->bit_2=1;break;
		case 3: set->bit_3=1;break;
		case 4: set->bit_4=1;break;
		case 5: set->bit_5=1;break;
		case 6: set->bit_6=1;break;
		case 7: set->bit_7=1;break;
	}
	return(tmp);
}/*set_set*/
/*
 		#] set_set :
 		#[ set_del :
			deletes ch from set; returns *set:
*/
one_byte 
set_del ARG2(UBYTE, ch, set_of_char, set)
{
	one_byte tmp=(one_byte)set;
	set += ch/8;
	switch (ch % 8){
		case 0: set->bit_0=0;break;
		case 1: set->bit_1=0;break;
		case 2: set->bit_2=0;break;
		case 3: set->bit_3=0;break;
		case 4: set->bit_4=0;break;
		case 5: set->bit_5=0;break;
		case 6: set->bit_6=0;break;
		case 7: set->bit_7=0;break;
	}
	return(tmp);
}/*set_del*/
/*
 		#] set_del :
 		#[ set_sub :
			returns *set = set1\set2. This function may be usd for initialising,
				set_sub(a,a,a) => now a is empty set :
*/
one_byte 
set_sub ARG3(set_of_char, set, set_of_char, set1, set_of_char, set2)
{
	one_byte tmp=(one_byte)set;
	int i=0,j=0;
	while(j=0,i++<32)
	while(j<9)
		switch (j++){
			case 0: set->bit_0=(set1->bit_0&&(!set2->bit_0));break;
			case 1: set->bit_1=(set1->bit_1&&(!set2->bit_1));break;
			case 2: set->bit_2=(set1->bit_2&&(!set2->bit_2));break;
			case 3: set->bit_3=(set1->bit_3&&(!set2->bit_3));break;
			case 4: set->bit_4=(set1->bit_4&&(!set2->bit_4));break;
			case 5: set->bit_5=(set1->bit_5&&(!set2->bit_5));break;
			case 6: set->bit_6=(set1->bit_6&&(!set2->bit_6));break;
			case 7: set->bit_7=(set1->bit_7&&(!set2->bit_7));break;
			case 8: set++;set1++;set2++;
     };
	return(tmp);
}/*set_sub*/
/*
 		#] set_sub :
*/
/*:[12dec2003 mt]*/

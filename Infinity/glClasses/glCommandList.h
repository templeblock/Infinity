class CommandList
{
	public:
		CommandList() {mHandle = glGenLists(1);}
		~CommandList() {glDeleteLists(mHandle, 1);}
		
		void begin() {glNewList(mHandle, GL_COMPILE);}
		void end() {glEndList();}

		void execute() {glCallList(mHandle);}

	public:
		GLuint mHandle;
};
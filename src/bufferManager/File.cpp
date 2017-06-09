#include "File.h"
#include "BufferManager.h"

File::File(const string &fileName)
    :Stream(fileName.c_str()),FileSize(Stream.size())
{
    Stream.open(QFile::ReadWrite|QFile::Unbuffered);
    BlockCount = Convert(FileSize);
}

Buffer *File::GetBuffer(const size_t &block, const pos_type &offset)
{
    Buffer *buffer;
    Mutex.lock();
    auto i = Buffers.find(block);
    if(i==Buffers.end())
    {
        pos_type pos = block * Buffer::bufferSize();
        buffer = BufferManager::bufferManager()
                .buff(this, pos, offset);
        Buffers.insert(std::make_pair(block,buffer));
        buffer->top();
        BufferManager::bufferManager()
                .preRead(this,pos + Buffer::bufferSize() );
    }
    else
    {
        buffer = i->second;
        i->second->top();
    }
    Mutex.unlock();
    if(block > BlockCount || buffer->Size < offset)
    {
        //resize(newBlock * Buffer::bufferSize() + offset);
        buffer->changeSize(offset);
        BlockCount = block;
    }
    return buffer;
}

//开始读取地址
File::pos_type File::GetPos(const pos_type &pos, const pos_type &size)
{
    pos_type offset = (pos + size) % Buffer::bufferSize();
    if(offset && offset < size)
        return pos + size - offset;
    else
        return pos;
}

File::~File()
{
    BufferManager::bufferManager().wait();
    for(auto &i : Buffers)
        delete i.second;
    Stream.close();
}

void File::resize(const pos_type &pos)
{
    size_t newBlock = Convert(pos);
    Mutex.lock();
    if(newBlock<BlockCount)
    {
        for(pos_type i = newBlock; i<=BlockCount;++i)
        {
            auto iter = Buffers.find(newBlock);
            if(iter!=Buffers.end())
            {
                delete iter->second;
                Buffers.erase(iter);
            }
        }
    }
    Mutex.unlock();
    BlockCount = newBlock;
    FileSize = pos;
}


void File::flush()
{
    Mutex.lock();
    for(auto i : Buffers)
        if(i.second->Dirty)
            BufferManager::bufferManager().queueBuff(i.second);
    Mutex.unlock();
}

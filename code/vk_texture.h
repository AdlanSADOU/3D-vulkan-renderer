

struct Texture
{
    const char   *name;
    VkImage       image;
    VmaAllocation allocation;
    VkImageView   view;
    VkFormat      format;

    int      width;
    int      height;
    uint32_t num_channels;
    uint32_t id;

    void Create(const char *filepath);
    void Destroy();
};


typedef struct {
	int usage;
	const char *name;
} hid_usage_page_t;

typedef struct {
	int page;
	int usage;
	const char *name;
} hid_usage_t;


extern const hid_usage_page_t hid_usage_pages[];
extern const hid_usage_t hid_usage[];

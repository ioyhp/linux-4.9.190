#ifndef LINUX_MM_INLINE_H
#define LINUX_MM_INLINE_H

#include <linux/huge_mm.h>
#include <linux/swap.h>
#ifdef CONFIG_AMLOGIC_MEMORY_EXTEND
#include <linux/page-isolation.h>
#endif /* CONFIG_AMLOGIC_MEMORY_EXTEND */

/**
 * page_is_file_cache - should the page be on a file LRU or anon LRU?
 * @page: the page to test
 *
 * Returns 1 if @page is page cache page backed by a regular filesystem,
 * or 0 if @page is anonymous, tmpfs or otherwise ram or swap backed.
 * Used by functions that manipulate the LRU lists, to sort a page
 * onto the right LRU list.
 *
 * We would like to get this info without a page flag, but the state
 * needs to survive until the page is last deleted from the LRU, which
 * could be as far down as __page_cache_release.
 */
static inline int page_is_file_cache(struct page *page)
{
	return !PageSwapBacked(page);
}

static __always_inline void __update_lru_size(struct lruvec *lruvec,
				enum lru_list lru, enum zone_type zid,
				int nr_pages)
{
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);

	__mod_node_page_state(pgdat, NR_LRU_BASE + lru, nr_pages);
	__mod_zone_page_state(&pgdat->node_zones[zid],
				NR_ZONE_LRU_BASE + lru, nr_pages);
}

static __always_inline void update_lru_size(struct lruvec *lruvec,
				enum lru_list lru, enum zone_type zid,
				int nr_pages)
{
	__update_lru_size(lruvec, lru, zid, nr_pages);
#ifdef CONFIG_MEMCG
	mem_cgroup_update_lru_size(lruvec, lru, zid, nr_pages);
#endif
}

static __always_inline void add_page_to_lru_list(struct page *page,
				struct lruvec *lruvec, enum lru_list lru)
{
#ifdef CONFIG_AMLOGIC_MEMORY_EXTEND
	int nr_pages = hpage_nr_pages(page);
	int num = NR_INACTIVE_ANON_CMA - NR_INACTIVE_ANON;
	int migrate_type = 0;
#endif /* CONFIG_AMLOGIC_MEMORY_EXTEND */

	update_lru_size(lruvec, lru, page_zonenum(page), hpage_nr_pages(page));
	list_add(&page->lru, &lruvec->lists[lru]);

#ifdef CONFIG_AMLOGIC_MEMORY_EXTEND
	migrate_type = get_pageblock_migratetype(page);
	if (is_migrate_cma(migrate_type) || is_migrate_isolate(migrate_type))
		__mod_zone_page_state(page_zone(page),
				      NR_LRU_BASE + lru + num, nr_pages);
#endif /* CONFIG_AMLOGIC_MEMORY_EXTEND */
}

static __always_inline void del_page_from_lru_list(struct page *page,
				struct lruvec *lruvec, enum lru_list lru)
{
#ifdef CONFIG_AMLOGIC_MEMORY_EXTEND
	int nr_pages = hpage_nr_pages(page);
	int num = NR_INACTIVE_ANON_CMA - NR_INACTIVE_ANON;
	int migrate_type = 0;
#endif /* CONFIG_AMLOGIC_MEMORY_EXTEND */

	list_del(&page->lru);
	update_lru_size(lruvec, lru, page_zonenum(page), -hpage_nr_pages(page));

#ifdef CONFIG_AMLOGIC_MEMORY_EXTEND
	migrate_type = get_pageblock_migratetype(page);
	if (is_migrate_cma(migrate_type) || is_migrate_isolate(migrate_type))
		__mod_zone_page_state(page_zone(page),
				      NR_LRU_BASE + lru + num, -nr_pages);
#endif /* CONFIG_AMLOGIC_MEMORY_EXTEND */
}

/**
 * page_lru_base_type - which LRU list type should a page be on?
 * @page: the page to test
 *
 * Used for LRU list index arithmetic.
 *
 * Returns the base LRU type - file or anon - @page should be on.
 */
static inline enum lru_list page_lru_base_type(struct page *page)
{
	if (page_is_file_cache(page))
		return LRU_INACTIVE_FILE;
	return LRU_INACTIVE_ANON;
}

/**
 * page_off_lru - which LRU list was page on? clearing its lru flags.
 * @page: the page to test
 *
 * Returns the LRU list a page was on, as an index into the array of LRU
 * lists; and clears its Unevictable or Active flags, ready for freeing.
 */
static __always_inline enum lru_list page_off_lru(struct page *page)
{
	enum lru_list lru;

	if (PageUnevictable(page)) {
		__ClearPageUnevictable(page);
		lru = LRU_UNEVICTABLE;
	} else {
		lru = page_lru_base_type(page);
		if (PageActive(page)) {
			__ClearPageActive(page);
			lru += LRU_ACTIVE;
		}
	}
	return lru;
}

/**
 * page_lru - which LRU list should a page be on?
 * @page: the page to test
 *
 * Returns the LRU list a page should be on, as an index
 * into the array of LRU lists.
 */
static __always_inline enum lru_list page_lru(struct page *page)
{
	enum lru_list lru;

	if (PageUnevictable(page))
		lru = LRU_UNEVICTABLE;
	else {
		lru = page_lru_base_type(page);
		if (PageActive(page))
			lru += LRU_ACTIVE;
	}
	return lru;
}

#define lru_to_page(head) (list_entry((head)->prev, struct page, lru))

#endif

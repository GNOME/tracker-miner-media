/*
 * Copyright (C) 2014 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "tracker-miner-media.h"

#include <gdata/gdata.h>

#define TMM_DATA_SOURCE "tmm:urn:83443497-b4cf-4341-8ac8-74058828f6db"
#define TMM_GRAPH "tmm:graph:33091b97-fc29-431e-8747-a62b3f3ec56f"

typedef struct _FileInfo FileInfo;
typedef struct _TmmDecoratorPrivate TmmDecoratorPrivate;

struct _FileInfo
{
	GFile *file;
	TmmDecorator *decorator;
	TrackerSparqlBuilder *sparql;
	GTask *task;

	gchar *urn;
	gchar *freebase_id;

	gchar *title;
	gint season;
	gint episode;
};

struct _TmmDecoratorPrivate
{
	GDataFreebaseService *freebase_service;
	GCancellable *cancellable;
};

G_DEFINE_TYPE_WITH_PRIVATE (TmmDecorator, tmm_decorator, TRACKER_TYPE_DECORATOR_FS)
G_DEFINE_QUARK (TmmDecoratorError, tmm_decorator_error);

static FileInfo *
file_info_new (GFile        *file,
               TmmDecorator *decorator,
               GTask        *task,
               const gchar  *urn)
{
	FileInfo *info;

	info = g_new0 (FileInfo, 1);
	info->file = g_object_ref (file);
	info->decorator = decorator;
	info->sparql = g_task_get_task_data (task);
	info->task = task;
	info->urn = g_strdup (urn);

	return info;
}

static void
file_info_free (FileInfo *info)
{
	g_object_unref (info->file);
	g_free (info->freebase_id);
	g_free (info->title);
	g_free (info);
}

static gboolean
file_info_is_episode (FileInfo *info)
{
	return info->season > 0 && info->episode > 0;
}

static void
sparql_builder_object_time (TrackerSparqlBuilder *sparql,
                            gint64                time)
{
	struct tm utc_time;
	gchar buffer[30];

	gmtime_r (&time, &utc_time);
	strftime (buffer, sizeof (buffer), "%FT%TZ", &utc_time);
	tracker_sparql_builder_object_string (sparql, buffer);
}

GPtrArray *
file_info_extract_artists (GDataFreebaseTopicObject *object,
                           TrackerSparqlBuilder     *sparql,
                           const gchar              *freebase_property)
{
	GPtrArray *artists;
	gint64 i;

	artists = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	for (i = 0; i < gdata_freebase_topic_object_get_property_count (object, freebase_property); i++) {
		GDataFreebaseTopicValue *value;
		const gchar *artist_name;
		gchar *urn;

		value = gdata_freebase_topic_object_get_property_value (object, freebase_property, i);
		artist_name = gdata_freebase_topic_value_get_text (value);
		urn = tracker_sparql_escape_uri_printf ("urn:artist:%s", artist_name);

		tracker_sparql_builder_insert_open (sparql, NULL);
		tracker_sparql_builder_subject_iri (sparql, urn);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nmm:Artist");
		tracker_sparql_builder_predicate (sparql, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (sparql, artist_name);
		tracker_sparql_builder_insert_close (sparql);

		g_ptr_array_add (artists, urn);
	}

	return artists;
}

GPtrArray *
file_info_extract_actors (GDataFreebaseTopicObject *object,
                          TrackerSparqlBuilder     *sparql)
{
	GPtrArray *artists;
	gint64 i;

	artists = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	for (i = 0; i < gdata_freebase_topic_object_get_property_count (object, "/film/film/starring"); i++) {
		GDataFreebaseTopicValue *value, *child_value;
		const GDataFreebaseTopicObject *child_object;
		const gchar *artist_name;
		gchar *urn;

		value = gdata_freebase_topic_object_get_property_value (object, "/film/film/starring", i);
		child_object = gdata_freebase_topic_value_get_object (value);
		child_value = gdata_freebase_topic_object_get_property_value (child_object, "/film/performance/actor", 0);

		artist_name = gdata_freebase_topic_value_get_text (child_value);
		urn = tracker_sparql_escape_uri_printf ("urn:artist:%s", artist_name);

		tracker_sparql_builder_insert_open (sparql, NULL);
		tracker_sparql_builder_subject_iri (sparql, urn);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "nmm:Artist");
		tracker_sparql_builder_predicate (sparql, "nmm:artistName");
		tracker_sparql_builder_object_unvalidated (sparql, artist_name);
		tracker_sparql_builder_insert_close (sparql);

		g_ptr_array_add (artists, urn);
	}

	return artists;
}

static void
file_info_fail (FileInfo *info,
                GError   *error)
{
	g_task_return_error (info->task, error);
}

static void
file_info_extract (FileInfo                 *info,
                   GDataFreebaseTopicResult *result)
{
	GDataFreebaseTopicValue *value, *child_value;
	GPtrArray *directors, *producers, *actors;
	const GDataFreebaseTopicObject *object;
	GDataFreebaseTopicObject *root;
	const gchar *title = NULL;
	gint64 i, creation_time = -1;

	root = gdata_freebase_topic_result_dup_object (result);
	directors = producers = actors = NULL;

	if (file_info_is_episode (info)) {
		directors = file_info_extract_artists (root, info->sparql, "/tv/tv_series_episode/director");
		producers = file_info_extract_artists (root, info->sparql, "tv/tv_series_episode/producers");
	} else {
		directors = file_info_extract_artists (root, info->sparql, "/film/film/directed_by");
		producers = file_info_extract_artists (root, info->sparql, "/film/film/produced_by");
		actors = file_info_extract_actors (root, info->sparql);
	}

	/* Extract title */
	value = gdata_freebase_topic_object_get_property_value (root, "/type/object/name", 0);

	if (value)
		title = gdata_freebase_topic_value_get_string (value);

	/* Extract release date */
	if (file_info_is_episode (info)) {
		value = gdata_freebase_topic_object_get_property_value (root, "/tv/tv_series_episode/air_date", 0);
	} else {
		value = gdata_freebase_topic_object_get_property_value (root, "/film/film/initial_release_date", 0);
	}

	if (value) {
		creation_time = gdata_freebase_topic_value_get_int (value);
	}

	/* Delete previous data, to be replaced by new info */
	if (title) {
		tracker_sparql_builder_delete_open (info->sparql, NULL);
		tracker_sparql_builder_subject_iri (info->sparql, info->urn);
		tracker_sparql_builder_predicate (info->sparql, "nie:title");
		tracker_sparql_builder_object_variable (info->sparql, "unknown");
		tracker_sparql_builder_delete_close (info->sparql);
	}

	if (creation_time > 0) {
		tracker_sparql_builder_delete_open (info->sparql, NULL);
		tracker_sparql_builder_subject_iri (info->sparql, info->urn);
		tracker_sparql_builder_predicate (info->sparql, "nie:contentCreated");
		tracker_sparql_builder_object_variable (info->sparql, "unknown");
		tracker_sparql_builder_delete_close (info->sparql);
	}

	tracker_sparql_builder_insert_open (info->sparql, NULL);
	tracker_sparql_builder_graph_open (info->sparql, TMM_GRAPH);
	tracker_sparql_builder_subject_iri (info->sparql, info->urn);

	/* Data source */
	tracker_sparql_builder_predicate (info->sparql, "a");
	tracker_sparql_builder_object (info->sparql, "nmm:Video");

	tracker_sparql_builder_predicate (info->sparql, "nie:dataSource");
	tracker_sparql_builder_object_iri (info->sparql,
	                                   tracker_decorator_get_data_source (TRACKER_DECORATOR (info->decorator)));

	if (title) {
		tracker_sparql_builder_predicate (info->sparql, "nie:title");
		tracker_sparql_builder_object_string (info->sparql, title);
	}

	if (creation_time > 0) {
		tracker_sparql_builder_predicate (info->sparql, "nie:contentCreated");
		sparql_builder_object_time (info->sparql, creation_time);
	}

	/* Text/synopsis */
	value = gdata_freebase_topic_object_get_property_value (root, "/common/topic/description", 0);
	tracker_sparql_builder_predicate (info->sparql, "nmm:synopsis");
	tracker_sparql_builder_object_string (info->sparql,
	                                      gdata_freebase_topic_value_get_string (value));

	if (file_info_is_episode (info)) {
		tracker_sparql_builder_predicate (info->sparql, "nmm:isSeries");
		tracker_sparql_builder_object_boolean (info->sparql, TRUE);

		value = gdata_freebase_topic_object_get_property_value (root, "/tv/tv_series_episode/season_number", 0);
		tracker_sparql_builder_predicate (info->sparql, "nmm:season");
		tracker_sparql_builder_object_int64 (info->sparql,
		                                     (gint64) gdata_freebase_topic_value_get_double (value));

		value = gdata_freebase_topic_object_get_property_value (root, "/tv/tv_series_episode/episode_number", 0);
		tracker_sparql_builder_predicate (info->sparql, "nmm:episodeNumber");
		tracker_sparql_builder_object_int64 (info->sparql,
		                                     (gint64) gdata_freebase_topic_value_get_double (value));
	} else {
		/* MPAA rating */
		value = gdata_freebase_topic_object_get_property_value (root, "/film/film/rating", 0);

		if (value) {
			tracker_sparql_builder_predicate (info->sparql, "nmm:MPAARating");
			tracker_sparql_builder_object_string (info->sparql,
			                                      gdata_freebase_topic_value_get_text (value));
		}

		/* Runtime */
		value = gdata_freebase_topic_object_get_property_value (root, "/film/film/runtime", 0);
		object = gdata_freebase_topic_value_get_object (value);
		child_value = gdata_freebase_topic_object_get_property_value (object, "/film/film_cut/runtime", 0);
		tracker_sparql_builder_predicate (info->sparql, "nmm:runTime");
		tracker_sparql_builder_object_int64 (info->sparql,
		                                     (gint64) gdata_freebase_topic_value_get_double (child_value));

		/* Genre(s) */
		for (i = 0; i < gdata_freebase_topic_object_get_property_count (root, "/film/film/genre"); i++) {
			value = gdata_freebase_topic_object_get_property_value (root, "/film/film/genre", i);
			tracker_sparql_builder_predicate (info->sparql, "nmm:genre");
			tracker_sparql_builder_object_string (info->sparql,
			                                      gdata_freebase_topic_value_get_text (value));

			/* FIXME: nmm:genre cardinality is 1 */
			break;
		}
	}

	/* Director(s) */
	for (i = 0; directors && i < directors->len; i++) {
		tracker_sparql_builder_predicate (info->sparql, "nmm:director");
		tracker_sparql_builder_object_iri (info->sparql, g_ptr_array_index (directors, i));
	}

	/* Producers(s) */
	for (i = 0; producers && i < producers->len; i++) {
		tracker_sparql_builder_predicate (info->sparql, "nmm:producedBy");
		tracker_sparql_builder_object_iri (info->sparql, g_ptr_array_index (producers, i));
		/* FIXME: nmm:producedBy cardinality is 1 */
		break;
	}

	/* Actors */
	for (i = 0; actors && i < actors->len; i++) {
		tracker_sparql_builder_predicate (info->sparql, "nmm:leadActor");
		tracker_sparql_builder_object_iri (info->sparql, g_ptr_array_index (actors, i));
	}

	gdata_freebase_topic_object_unref (root);

	tracker_sparql_builder_graph_close (info->sparql);
        tracker_sparql_builder_insert_close (info->sparql);

	g_clear_pointer (&directors, (GDestroyNotify) g_ptr_array_unref);
	g_clear_pointer (&producers, (GDestroyNotify) g_ptr_array_unref);
	g_clear_pointer (&actors, (GDestroyNotify) g_ptr_array_unref);
}

static void decorator_get_next_item_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data);

static void
file_info_topic_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
	GDataFreebaseTopicResult *topic_result;
	FileInfo *info = user_data;
	GError *error = NULL;

	topic_result =
		GDATA_FREEBASE_TOPIC_RESULT (gdata_service_query_single_entry_finish (GDATA_SERVICE (object),
		                                                                      result, &error));
	if (error) {
		gchar *uri;

		uri = g_file_get_uri (info->file);
		g_warning ("Could not lookup topic for '%s' (Freebase ID: %s)",
		           uri, info->freebase_id);
		g_free (uri);

		file_info_fail (info, error);
	} else {
		g_debug ("Extracting info for '%s'", info->urn);
		file_info_extract (info, topic_result);
		g_object_unref (topic_result);

		g_task_return_boolean (info->task, TRUE);
	}

	tracker_decorator_next (TRACKER_DECORATOR (info->decorator),
	                        NULL, decorator_get_next_item_cb, NULL);
}

static void
file_info_get_topic (FileInfo *info)
{
	GDataFreebaseTopicQuery *topic_query;
	TmmDecoratorPrivate *priv;

	g_assert (info->freebase_id);
	priv = tmm_decorator_get_instance_private (info->decorator);

	g_debug ("Item '%s' being queried as '%s'",
	         info->title, info->freebase_id);

	topic_query = gdata_freebase_topic_query_new (info->freebase_id);
	gdata_freebase_service_get_topic_async (priv->freebase_service,
	                                        topic_query,
	                                        priv->cancellable,
	                                        file_info_topic_cb,
	                                        info);
	g_object_unref (topic_query);
}

static gchar *
variant_extract_id (GVariant *variant)
{
	GVariantIter iter;
	gchar *id = NULL;
	GVariant *value;
	gchar *key;

	g_variant_iter_init (&iter, variant);

	if (g_variant_iter_n_children (&iter) == 0)
		return NULL;

	while (g_variant_iter_loop (&iter, "{sv}", &key, &value)) {
		if (strcmp (key, "id") != 0)
			continue;

		id = g_variant_dup_string (value, NULL);

		/* Free key/value, as we break the loop */
		g_variant_unref (value);
		g_free (key);
		break;
	}

	return id;
}

static void
mql_query_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
	GDataFreebaseResult *mql_result;
	FileInfo *info = user_data;
	GError *error = NULL;
	GVariant *variant;

	mql_result =
		GDATA_FREEBASE_RESULT (gdata_service_query_single_entry_finish (GDATA_SERVICE (object),
		                                                                result, &error));

	if (error) {
		g_warning ("Could not perform MQL query to Freebase: %s", error->message);

		file_info_fail (info, error);
		tracker_decorator_next (TRACKER_DECORATOR (info->decorator),
		                        NULL, decorator_get_next_item_cb, NULL);
		return;
	}

	variant = gdata_freebase_result_dup_variant (mql_result);
	g_object_unref (mql_result);

	info->freebase_id = variant_extract_id (variant);
	g_variant_unref (variant);

	if (info->freebase_id) {
		file_info_get_topic (info);
	} else {
		error = g_error_new (tmm_decorator_error_quark (), 0, "MQL search returned no items");
		file_info_fail (info, error);
		tracker_decorator_next (TRACKER_DECORATOR (info->decorator),
		                        NULL, decorator_get_next_item_cb, NULL);
	}
}

static void
search_query_cb (GObject      *object,
		 GAsyncResult *result,
		 gpointer      user_data)
{
	const GDataFreebaseSearchResultItem *item = NULL;
	GDataFreebaseSearchResult *search_result;
	FileInfo *info = user_data;
	GError *error = NULL;

	search_result =
		GDATA_FREEBASE_SEARCH_RESULT (gdata_service_query_single_entry_finish (GDATA_SERVICE (object),
		                                                                       result, &error));

	if (error) {
		g_warning ("Could not search in Freebase: %s", error->message);
		g_error_free (error);

		file_info_fail (info, error);
		tracker_decorator_next (TRACKER_DECORATOR (info->decorator),
		                        NULL, decorator_get_next_item_cb, NULL);
		return;
	}

	if (gdata_freebase_search_result_get_num_items (search_result) > 0)
		item = gdata_freebase_search_result_get_item (search_result, 0);

#define MIN_SCORE 50
	if (item && gdata_freebase_search_result_item_get_score (item) > MIN_SCORE) {
		info->freebase_id = g_strdup (gdata_freebase_search_result_item_get_id (item));
		file_info_get_topic (info);
	} else {
		GError *error;
		gchar *uri;

		uri = g_file_get_uri (info->file);
		g_debug ("Found no search results for '%s'", uri);
		g_free (uri);

		error = g_error_new (tmm_decorator_error_quark (), 0, "No result items");

		file_info_fail (info, error);
		tracker_decorator_next (TRACKER_DECORATOR (info->decorator),
		                        NULL, decorator_get_next_item_cb, NULL);
	}
#undef MIN_SCORE

	g_object_unref (search_result);
}

static gboolean
str_is_digit (const gchar *str)
{
	gint i;

	for (i = 0; str[i]; i++) {
		if (!g_ascii_isdigit (str[i]))
			return FALSE;
	}

	return TRUE;
}

static gboolean
str_is_year (const gchar *str)
{
	return str_is_digit (str) && strlen (str) == 4 &&
		/* Screw you, 22nd century! */
		((str[0] == '1' && str[1] == '9') ||
		 (str[0] == '2' && str[1] == '0'));
}

static void
file_info_guess (FileInfo     *info,
                 const gchar **title,
                 gint         *season,
                 gint         *episode)
{
	gchar *filename, *ext;
	gboolean add = TRUE;
	GString *string;
	gchar **tokens;
	gint i, s, ep;

	s = ep = 0;
	filename = g_file_get_basename (info->file);
	ext = strrchr (filename, '.');

	if (ext)
		ext[0] = '\0';

	string = g_string_new ("");
	tokens = g_strsplit_set (filename, ". ", -1);

	for (i = 0; tokens[i]; i++) {
		if (ep == 0 &&
		    (sscanf (tokens[i], "S%dE%d", &s, &ep) == 2 ||
		     sscanf (tokens[i], "s%de%d", &s, &ep) == 2 ||
		     sscanf (tokens[i], "%dx%d", &s, &ep) == 2)) {
			add = FALSE;
		} else if (string->len > 0 &&
		           (str_is_year (tokens[i]) ||
		            *tokens[i] == '[' || *tokens[i] == '(')) {
			add = FALSE;
		}

		if (add) {
			if (string->len > 0)
				g_string_append (string, " ");

			g_string_append (string, tokens[i]);
		}
	}

	info->title = g_string_free (string, FALSE);
	info->season = s;
	info->episode = ep;

	if (title)
		*title = info->title;

	if (season)
		*season = info->season;
	if (episode)
		*episode = info->episode;

	g_free (filename);
}

static void
file_info_search (FileInfo *info)
{
	TmmDecoratorPrivate *priv;
	gint season, episode;
	const gchar *title;
	GFile *file;

	g_assert (!info->freebase_id);
	priv = tmm_decorator_get_instance_private (info->decorator);

	g_debug ("Searching for information about '%s'", info->urn);

	file_info_guess (info, &title, &season, &episode);

	if (file_info_is_episode (info)) {
		GDataFreebaseQuery *mql_query;
		gchar *str;

		g_debug ("Guessed as series: '%s', season: %d, episode: %d",
		         title, season, episode);

		str = g_strdup_printf ("{ \"type\": \"/tv/tv_series_episode\","
		                       "  \"season_number\": %d,"
		                       "  \"episode_number\": %d,"
		                       "  \"series\": \"%s\","
		                       "  \"id\": null }",
		                       season, episode, title);

		mql_query = gdata_freebase_query_new (str);
		gdata_freebase_service_query_async (priv->freebase_service,
		                                    mql_query, priv->cancellable,
		                                    mql_query_cb, info);
		g_object_unref (mql_query);
		g_free (str);
	} else {
		GDataFreebaseSearchQuery *search_query;

		g_debug ("Guessed as film: '%s'", title);

		search_query = gdata_freebase_search_query_new (title);
		gdata_query_set_max_results (GDATA_QUERY (search_query), 1);

		gdata_freebase_search_query_open_filter (search_query, GDATA_FREEBASE_SEARCH_FILTER_ANY);
		gdata_freebase_search_query_add_filter (search_query, "type", "/film/film");
		gdata_freebase_search_query_close_filter (search_query);

		gdata_freebase_service_search_async (priv->freebase_service,
		                                     search_query,
		                                     priv->cancellable,
		                                     search_query_cb,
		                                     info);
		g_object_unref (search_query);
	}
}

static void
decorator_get_next_item_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
	TrackerDecorator *decorator = TRACKER_DECORATOR (object);
	TrackerDecoratorInfo *info;
	FileInfo *file_info;
	GError *error = NULL;
	GFile *file;
	GTask *task;

	if (tracker_decorator_get_n_items (decorator) == 0)
		return;

	info = tracker_decorator_next_finish (decorator, result, &error);

	if (!info) {
		g_warning ("Next item could not be retrieved: %s\n",
		           error->message);
		g_error_free (error);
		return;
	}

	task = tracker_decorator_info_get_task (info);
	file = g_file_new_for_uri (tracker_decorator_info_get_url (info));

	file_info = file_info_new (file, TMM_DECORATOR (object), task,
	                           tracker_decorator_info_get_urn (info));
	g_object_unref (file);

	file_info_search (file_info);
}

static void
tmm_decorator_items_available (TrackerDecorator *decorator)
{
	tracker_decorator_next (decorator, NULL,
	                        decorator_get_next_item_cb,
	                        NULL);
}

static void
tmm_decorator_finished (TrackerDecorator *decorator)
{
}

static void
tmm_decorator_paused (TrackerMiner *miner)
{
	TmmDecoratorPrivate *priv;

	priv = tmm_decorator_get_instance_private (TMM_DECORATOR (miner));
	g_cancellable_cancel (priv->cancellable);
	g_cancellable_reset (priv->cancellable);
}

static void
tmm_decorator_finalize (GObject *object)
{
	TmmDecoratorPrivate *priv;

	priv = tmm_decorator_get_instance_private (TMM_DECORATOR (object));
	g_object_unref (priv->freebase_service);
}

static void
tmm_decorator_class_init (TmmDecoratorClass *klass)
{
	TrackerDecoratorClass *decorator_class = TRACKER_DECORATOR_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	miner_class->paused = tmm_decorator_paused;

	decorator_class->items_available = tmm_decorator_items_available;
	decorator_class->finished = tmm_decorator_finished;

	object_class->finalize = tmm_decorator_finalize;
}

static void
tmm_decorator_init (TmmDecorator *decorator)
{
	TmmDecoratorPrivate *priv;

	priv = tmm_decorator_get_instance_private (decorator);
	priv->freebase_service = gdata_freebase_service_new (NULL, NULL);
	priv->cancellable = g_cancellable_new ();
}

TrackerMiner *
tmm_decorator_new (void)
{
	gchar *classes[] = {
		"nfo:Video",
		NULL
	};

	return g_object_new (TMM_TYPE_DECORATOR,
	                     "name", "Media",
	                     "data-source", TMM_DATA_SOURCE,
	                     "class-names", classes,
	                     NULL);
}

<?php

// $Id$

/**
 * The main classes for the scale question type.
 *
 * These inherit or implement code found in quiz_question.classes.inc.
 *
 * Sponsored by: Norwegian Centre for Telemedicine
 * Code: falcon
 *
 * Based on:
 * Other question types in the quiz framework.
 * 
 * 
 *
 * @file
 * Question type, enabling rapid creation of surveys using the quiz framework.
 */

/*
 * 
 * In the quiz framework:
 * TODO: Make it possible to generate links to a survey taking page.
 */

/**
 * Implementation of QuizQuestion.
 */
class ScaleQuestion implements QuizQuestion {

  /**
   * The current node for this question.
   */
  protected $node = NULL;
  protected $util = FALSE; //The class is used only as a utility.
  protected $col_id = NULL;
  /**
   * Constructor
   * 
   * @param $node - the node object
   */
  public function __construct($node) {
    $this->node = $node;
  }
  /**
   * Tells the object that it is beeing used as a utility.
   * 
   * @param $c_id - answer collection id
   */
  public function initUtil($c_id) {
    $this->util = TRUE;
    $this->col_id = $c_id;
  }
  
  /**
   * Implementation of save
   * 
   * Stores the question in the database.
   * 
   * @param is_new if - if the node is a new node...
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/QuizQuestion#save()
   */
  public function save($is_new = FALSE) {
    global $user;
    $is_new_node = $is_new || $this->node->revision == 1;
    $answer_collection_id = $this->saveAnswerCollection($is_new_node);
    if ($this->node->save == 1) {
      $this->setPreset($answer_collection_id);
    }
    if ($is_new_node) {
      $sql = 'INSERT INTO {quiz_scale_node_properties}
        (nid, vid, answer_collection_id)
        VALUES (%d, %d, %d)';
      db_query($sql, $this->node->nid, $this->node->vid, $answer_collection_id);
    } 
    else {
      $sql = 'UPDATE {quiz_scale_node_properties}
        SET answer_collection_id = %d
        WHERE nid = %d AND vid = %d';
      db_query($sql, $answer_collection_id, $this->node->nid, $this->node->vid);
    }
  }
  /**
   * Add a preset for the current user.
   * 
   * @param $col_id - answer collection id of the collection this user wants to have as a preset
   */
  private function setPreset($col_id) {
    global $user;
    $sql = 'INSERT IGNORE INTO {quiz_scale_user} 
            (uid, answer_collection_id) 
            VALUES (%d, %d)';
    db_query($sql, $user->uid, $col_id);
  }
  /**
   * Stores the answer collection to the database, or reuses an excisting collection.
   * 
   * @param $is_new_node - the question is beeing inserted(not updated)
   * @param $alt_input - the alternatives array to be saved.
   * @param $preset - 1 | 0 = preset | not preset
   * @return unknown_type
   */
  public function saveAnswerCollection($is_new_node, $alt_input = NULL, $preset = NULL) {
    global $user;
    if (!isset($alt_input)) $alt_input = get_object_vars($this->node);
    if (!isset($preset)) $preset = $this->node->save;
    $alternatives = array();
    // Fetching alternatives
    for ($i = 0; $i < variable_get('scale_max_num_of_alts', 10); $i++) {
      if (strlen($alt_input['alternative'. $i]) > 0) {
        $alternatives[] = $alt_input['alternative'. $i];
      }
    }
    if ($answer_collection_id = $this->excistingCollection($alternatives)) {
      if ($preset == 1) {
        $this->setPreset($answer_collection_id);
      } 
      if (!$is_new_node || $this->util) {
        $col_to_delete = $this->util ? $this->col_id : $this->node->{0}->answer_collection_id;
        if ($col_to_delete != $answer_collection_id) {
          $this->deleteCollectionIfNotUsed($col_to_delete, 1);
        }
      }
      return $answer_collection_id;
    }
    db_query('INSERT INTO {quiz_scale_answer_collection} () VALUES ()');
    $answer_collection_id = db_last_insert_id('quiz_scale_answer_collection', 'id');
    if ($preset == 1) {
      $sql = 'INSERT INTO {quiz_scale_user} 
              (uid, answer_collection_id) 
              VALUES (%d, %d)';
      db_query($sql, $user->uid, $answer_collection_id);
    }
    db_lock_table('quiz_scale_answer');
    for ($i = 0; $i < count($alternatives); $i++) {
      $this->saveAlternative($alternatives[$i], $answer_collection_id);
    }
    db_unlock_tables();
    return $answer_collection_id;
  }
  /**
   * Saves one alternative to the database
   * 
   * @param $alternative - the alternative(String) to be saved.
   * @param $answer_collection_id - the id of the answer collection this alternative shall belong to.
   */
  private function saveAlternative($alternative, $answer_collection_id) {
    $sql = 'INSERT INTO {quiz_scale_answer}
    		(answer_collection_id, answer)
    		VALUES (%d, \'%s\')';
    db_query($sql, $answer_collection_id, $alternative);
  }
  /**
   * Deletes an answer collection if it isn't beeing used.
   * 
   * @param $answer_collection_id 
   * @param $accept - if collection is used more than this many times we keep it.
   * @return true if deleted, false if not deleted.
   */
  public function deleteCollectionIfNotUsed($answer_collection_id, $accept = 0) {
    $sql = 'SELECT COUNT(*) 
    		FROM {quiz_scale_user}
    		WHERE answer_collection_id = %d';
    $results = db_fetch_array(db_query($sql, $answer_collection_id));
    if ($results['COUNT(*)'] > 0) return;
    $sql = 'SELECT for_all 
    		FROM {quiz_scale_answer_collection}
    		WHERE id = %d';
    $results = db_fetch_object(db_query($sql, $answer_collection_id));
    if ($results->for_all == 1) return;
    $sql = 'SELECT COUNT(*)
    		FROM {quiz_scale_node_properties}
    		WHERE answer_collection_id = %d';
    $results = db_fetch_array(db_query($sql, $answer_collection_id));
    if ($results['COUNT(*)'] <= $accept) {
      $sql = 'DELETE FROM {quiz_scale_answer_collection}
      		  WHERE id = %d';
      db_query($sql, $answer_collection_id);
      $sql = 'DELETE FROM {quiz_scale_answer}
      		  WHERE answer_collection_id = %d';
      db_query($sql, $answer_collection_id);
      return TRUE;
    }
    return FALSE;
  }
  /**
   * Finds out if a collection allready exists.
   * 
   * @param $alternatives - this is the collection that will be compared with the database.
   * @param $answer_collection_id - if we are matching a set of alternatives with a given collection.
   * @param $last_id - The id of the last alternative we compared with.
   * @return unknown_type
   */
  private function excistingCollection($alternatives, $answer_collection_id = NULL, $last_id = NULL) {
    $my_alts = isset($answer_collection_id) ? $alternatives : array_reverse($alternatives);
    $sql = 'SELECT id, answer_collection_id
    		FROM {quiz_scale_answer}
    		WHERE answer = \'%s\'';
    if (isset($answer_collection_id)) $sql .= ' AND answer_collection_id = %d';
    if (isset($last_id)) $sql .= ' AND id = %d';
    $res = db_query($sql, array_pop($my_alts), $answer_collection_id, ($last_id + 1));
    if (!$res_o = db_fetch_object($res)) return FALSE;
    if (count($my_alts) == 0) {
      $sql = 'SELECT * FROM {quiz_scale_answer} 
              WHERE answer_collection_id = %d
              AND id = %d';
      $res_o2 = db_fetch_object(db_query($sql, $answer_collection_id, ($last_id + 2)));
      return ($res_o2 == FALSE) ? $answer_collection_id : FALSE;
    } 
    do {
      $col_id = $this->excistingCollection($my_alts, $res_o->answer_collection_id, $res_o->id);
      if ($col_id) return $col_id;
    } while ($res_o = db_fetch_object($res));
    return FALSE;
  }
  /**
   * Implementation of validate
   * 
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/QuizQuestion#validate()
   */
  public function validate($node, &$form) { 
  }
  /**
   * Implementation of delete
   * 
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/QuizQuestion#delete()
   */
  public function delete($only_this_version = FALSE) {
    if ($only_this_version) {
      db_query('DELETE FROM {quiz_scale_node_properties} WHERE nid = %d AND vid = %d', $this->node->nid, $this->node->vid);
      $sql = 'DELETE FROM {quiz_scale_user_answers} 
      		  WHERE result_id IN(
      		  	SELECT result_id 
      		  	FROM {quiz_node_results} 
      		  	WHERE nid = %d AND vid = %d
      		  )';
      db_query($sql, $this->node->nid, $this->node->vid);
    }
    else {
      db_query('DELETE FROM {quiz_scale_node_properties} WHERE nid = %d', $this->node->nid);
      $sql = 'DELETE FROM {quiz_scale_user_answers} 
      		  WHERE result_id IN(
      		  	SELECT result_id 
      		  	FROM {quiz_node_results} 
      		  	WHERE nid = %d
      		  )';
      db_query($sql, $this->node->nid);
    }
    $this->deleteCollectionIfNotUsed($this->node->{0}->answer_collection_id, 0);
  }

  /**
   * Implementation of load
   * 
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/QuizQuestion#load()
   */
  public function load() {
    $to_return = array();
    $sql = 'SELECT id, answer, a.answer_collection_id
            FROM {quiz_scale_node_properties} p
            JOIN {quiz_scale_answer} a ON (p.answer_collection_id = a.answer_collection_id)
            WHERE nid = %d AND vid = %d';
    $res = db_query($sql, $this->node->nid, $this->node->vid);
    while ($res_o = db_fetch_object($res)) {
      $to_return[] = $res_o;
    }
    return $to_return;
  }
  /**
   * Implementation of view
   * 
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/QuizQuestion#view()
   */
  public function view() {
    return $this->getQuestionForm($this->node);
  }

  /**
   * Generates the question form.
   * 
   * This is called whenever a question is rendered, either
   * to an administrator or to a quiz taker.
   */
  
  public function getQuestionForm($node, $context = NULL) {
    
    $options = array();
    for ($i = 0; $i < variable_get('scale_max_num_of_alts', 10); $i++) {
      if (strlen($node->{$i}->answer)>0) {
        $options[$node->{$i}->id] = check_plain($node->{$i}->answer);
      }
    }
    $form['question'] = array(
      '#type' => 'markup',
      '#value' => check_markup($node->body, $node->format, FALSE),
    );
    $form['tries'] = array(
      '#type' => 'radios',
      '#title' => t('Choose one'),
      '#options' => $options,
    );
    return $form;
  }
  /**
   * Implementation of getAdminForm
   * 
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/QuizQuestion#getAdminForm()
   */
  public function getAdminForm($edit = NULL) {
    $form['manage'] = array(
      '#type' => 'markup',
      '#value' => l(t('Manage presets'), 'scale/collection/manage'),
    );
    $form['scale_max_num_of_alts'] = array(
      '#type' => 'textfield',
      '#title' => t('Maximum number of alternatives allowed'),
      '#default_value' => variable_get('scale_max_num_of_alts', 10),
    );
    return system_settings_form($form);
  }//TODO: Add validation
  /**
   * Implementation of getCreation form
   * 
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/QuizQuestion#getCreationForm()
   */
  public function getCreationForm($edit) {
    $form = array();
    /*
     * Getting presets from the database
     */
    $collections = $this->getPresetCollections(TRUE);
    $options = $this->makeOptions($collections);
    $options['d'] = '-';
    $jsArray = $this->makeJSArray($collections);
    
    $form['answer'] = array(
      '#type' => 'fieldset',
      '#title' => t('Answer'),
      '#description' => t('Provide alternatives for the user to answer.'),
      '#collapsible' => TRUE,
      '#collapsed' => FALSE,
    );
    $form['answer']['#theme'][] = 'scale_creation_form';
    $form['answer']['presets'] = array(
      '#type' => 'select',
      '#title' => t('Presets'),
      '#options' => $options,
      '#default_value' => 'd',
      '#description' => t('Select a set of alternatives'),
      '#attributes' => array('onchange' => 'refreshAlternatives(this)'),
    );
    $form['jsArray'] = array(
      '#type' => 'markup',
      '#value' => "<SCRIPT type='text/javascript'>$jsArray</SCRIPT>",
    );
    $form['answer']['alternatives'] = array(
      '#type' => 'fieldset',
      '#title' => t('Alternatives'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    for ($i = 0; $i < variable_get('scale_max_num_of_alts', 10); $i++) {
      $form['answer']['alternatives']["alternative$i"] = array(
        '#type' => 'textfield',
        '#title' => t('Alternative !i', array('!i' => ($i+1))),
        '#size' => 60,
        '#maxlength' => 256,
        '#default_value' => $this->node->{$i}->answer,
        '#required' => $i < 2,
      );
    }
    $form['answer']['alternatives']['save'] = array(
      '#type' => 'checkbox',
      '#title' => t('Save as a new preset'),
      '#description' => t('Current alternatives will be saved as a new preset'),
      '#default_value' => FALSE,
    );
    $form['answer']['manage'] = array(
      '#type' => 'markup',
      '#value' => l(t('Manage presets'), 'scale/collection/manage'),
    );
    return $form;
  }
  /**
   * Get all available presets for the current user.
   * 
   * @param $with_defaults
   * @return array holding all the preset collections as an array of objects.
   *         each object in the array has the following properties:
   *           ->alternatives(array)
   *           ->name(string)
   *           ->for_all(int, 0|1)
   */
  public function getPresetCollections($with_defaults = FALSE) {
    global $user;
    $collections = array(); //option array for select form element.
    $scale_element_names = array();
    $sql = 'SELECT DISTINCT ac.id AS answer_collection_id, a.answer, ac.for_all
            FROM {quiz_scale_user} au
            JOIN {quiz_scale_answer_collection} ac ON(au.answer_collection_id = ac.id)
            JOIN {quiz_scale_answer} a ON(a.answer_collection_id = ac.id)
            WHERE au.uid = %d';    		  
    if ($with_defaults) {
      $sql .= ' OR ac.for_all = 1';
    }
    $sql .= ' ORDER BY au.answer_collection_id';
    $res = db_query($sql, $user->uid);
    $col_id;
    while (true) {
      if (!($res_o = db_fetch_object($res)) || ($res_o->answer_collection_id != $col_id)) {
        /*
         * We have gone through alle elements for one answer collection, 
         * and needs to store the answer collections name and id in the options array...
         */
        if (isset($col_id)) {
          $num_scale_elements = count($collections[$col_id]->alternatives);
          $collections[$col_id]->name = check_plain($collections[$col_id]->alternatives[0] .' - '. $collections[$col_id]->alternatives[$num_scale_elements-1] .' ('. $num_scale_elements .')');
        }
        if (!$res_o) break;
        $col_id = $res_o->answer_collection_id;
        if (!isset($collections[$col_id])) {
          $collections[$col_id] = new stdClass();
          $collections[$col_id]->alternatives = array();
          $collections[$col_id]->for_all = $res_o->for_all;
        } 
      }
      $collections[$col_id]->alternatives[] = check_plain($res_o->answer);
    }
    return $collections;
  }
  /**
   * Makes options array for form elements.
   * 
   * @param $collections - collections array, from getPresetCollections() for instance...
   * @return options array.
   */
  private function makeOptions($collections) {
    $options = array();
    foreach ($collections as $col_id => $obj) {
      $options[$col_id] = $obj->name;
    }
    return $options;
  }
  /**
   * Makes a javascript constructing an answer collection array.
   * 
   * @param $collections - collections array, from getPresetCollections() for instance...
   * @return javascript(string)
   */
  private function makeJSArray($collections) {
    $jsArray = 'scaleCollections = new Array();';
    foreach ($collections as $col_id => $obj) {
      if (is_array($collections[$col_id]->alternatives)) {
      $jsArray .= "scaleCollections[$col_id] = new Array();";
        foreach ($collections[$col_id]->alternatives as $alt_id => $text) {
          $jsArray .= "scaleCollections[$col_id][$alt_id] = '".check_plain($text)."';";
        }
      }
    }
    return $jsArray;
  }
  /**
   * Implementation of getMaximumScore.
   * 
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-DRUPAL-6--4/question_types/quiz_question/QuizQuestion#getMaximumScore()
   */
  public function getMaximumScore() {
    return 0;
  }
}

/**
 * The short answer question response class.
 */
class ScaleResponse extends AbstractQuizQuestionResponse {
  /**
   * ID of the answer.
   */
  protected $answer_id = 0;
  
  /**
   * Constructor
   * 
   * @param $rid - response_id
   * @param $question - as an object
   * @param $answer - answer_id
   */
  public function __construct($rid, $question, $answer = NULL) {
    $this->rid = $rid;
    $this->question = $question;
    if (isset($answer)) $this->answer_id = $answer;
    else {
      $sql = 'SELECT answer_id 
              FROM {quiz_scale_user_answers}
              WHERE result_id = %d AND question_nid = %d AND question_vid = %d';
      $res = db_query($sql, $rid, $this->question->nid, $this->question->vid);
      $res_o = db_fetch_object($res);
      $this->answer_id = $res_o->answer_id;
    }
    $sql = 'SELECT answer 
            FROM {quiz_scale_answer}
            WHERE id = %d';
    $res = db_query($sql, $this->answer_id);
    $res_o = db_fetch_object($res);
    $this->answer = check_plain($res_o->answer);
  }
  /**
   * Implementation of save
   */
  public function save() {
    $sql = "INSERT INTO {quiz_scale_user_answers}
      (answer_id, result_id, question_vid, question_nid)
      VALUES (%d, %d, %d, %d)";
    db_query($sql, $this->answer_id, $this->rid, $this->question->vid, $this->question->nid);
  }
  /**
   * Implementation of delete
   */
  public function delete() {
    $sql = 'DELETE FROM {quiz_scale_user_answers} 
    		WHERE result_id = %d AND question_nid = %d AND question_vid = %d';
    db_query($sql, $this->rid, $this->question->nid, $this->question->vid);
  }
  /**
   * Implementation of score
   * 
   * @return 0 - this question type is never scored.
   */
  public function score() {
    return 0;
  }
  /**
   * Implementation of isCorrect
   * 
   * @return 0 - this question type is never scored
   * (non-PHPdoc)
   * @see sites/all/modules/quiz-HEAD/question_types/quiz_question/AbstractQuizQuestionResponse#isCorrect()
   */
  public function isCorrect() {
    return TRUE;
  }
  /**
   * Implementation of getResponse
   * 
   * @return answer
   */
  public function getResponse() {
    return $this->answer;
  }
  /**
   * Implementation of format report.
   * 
   * @param $showpoints
   * @param $showfeedback
   * @return report as html
   */
  public function formatReport($showpoints = TRUE, $showfeedback = TRUE) {
    $slug = '<div class="quiz_summary_question"><span class="quiz_question_bullet">Q:</span> '.
      check_markup($this->question->body, $this->question->format, FALSE) .
      '</div>';
    $result = '<div class="quiz_answer_feedback">';
    $result .= t('You answered:');
    $result .= '<BR/>';
    $result .= check_plain($this->answer);
    $result .= '</div>';

    return $slug . $result;
  }
}
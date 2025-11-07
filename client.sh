#!/bin/bash

while true; do
    echo "Are you a student or instructor? (s/i) or type 'exit' to quit:"
    read role

    if [[ "$role" == "exit" ]]; then
        echo "Goodbye!"
        break
    fi

    if [[ "$role" == "s" || "$role" == "S" ]]; then
        echo "Enter your student ID:"
        read student_id

        while true; do
            echo ""
            echo "Choose an option:"
            echo "1. View all courses available"
            echo "2. View marks in a course"
            echo "3. Drop a course"
            echo "4. Enroll in a course"
            echo "5. Exit to main menu"
            read choice

            case $choice in
                1)
                    echo "Fetching all available courses..."
                    curl -i "http://localhost:8080/api?id=$student_id$&ptype=0&reqtype=0"
                    ;;
                2)
                    echo "Enter course ID:"
                    read course_id
                    echo "Fetching marks for student $student_id in course $course_id..."
                    curl -i "http://localhost:8080/api?id=$student_id&ptype=0&reqtype=1&cid=$course_id"
                    ;;
                3)
                    echo "Enter course ID to drop:"
                    read course_id
                    echo "Dropping course $course_id for student $student_id..."
                    curl -i -X DELETE "http://localhost:8080/api?id=$student_id&ptype=0&reqtype=0&cid=$course_id"
                    ;;
                4)
                    echo "Enter course ID to enroll:"
                    read course_id
                    echo "Enrolling student $student_id in course $course_id..."
                    curl -i -X POST "http://localhost:8080/api?id=$student_id&ptype=0&reqtype=0&cid=$course_id"
                    ;;
                5)
                    echo "Returning to main menu..."
                    break
                    ;;
                *)
                    echo "Invalid choice."
                    ;;
            esac
        done

    elif [[ "$role" == "i" || "$role" == "I" ]]; then
        echo "Enter your instructor ID:"
        read instructor_id

        while true; do
            echo ""
            echo "Choose an option:"
            echo "1. Remove a course"
            echo "2. Add a course"
            echo "3. Update marks of a student in a course"
            echo "4. Exit to main menu"
            read choice

            case $choice in
                1)
                    echo "Enter course ID to remove:"
                    read course_id
                    echo "Removing course $course_id..."
                    curl -i -X DELETE "http://localhost:8080/api?id=$instructor_id&ptype=1&reqtype=0&cid=$course_id"
                    ;;
                2)
                    echo "Enter course ID to add:"
                    read course_id
                    echo "Enter course name:"
                    read course_name
                    echo "Adding course $course_id ($course_name)..."
                    curl -i -X POST "http://localhost:8080/api?id=$instructor_id&ptype=1&reqtype=0&cid=$course_id" -d "$course_name"
                    ;;
                3)
                    echo "Enter course ID:"
                    read course_id
                    echo "Enter student ID:"
                    read student_id
                    echo "Enter marks:"
                    read marks
                    echo "Updating marks for student $student_id in course $course_id..."
                    curl -i -X PUT "http://localhost:8080/api?id=$instructor_id&ptype=0&reqtype=0&cid=$course_id&sid=$student_id&marks=$marks"
                    ;;
                4)
                    echo "Returning to main menu..."
                    break
                    ;;
                *)
                    echo "Invalid choice."
                    ;;
            esac
        done
    else
        echo "Invalid input. Please choose 's' for student, 'i' for instructor, or 'exit' to quit."
    fi

    echo ""
done
